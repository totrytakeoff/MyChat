#ifndef MULTI_PLATFORM_AUTH_HPP
#define MULTI_PLATFORM_AUTH_HPP

/******************************************************************************
 *
 * @file       multi_platform_auth.hpp
 * @brief      多平台认证管理器，负责跨平台用户认证和权限验证，实现基于JWT的双Token认证机制
 *             支持Access Token和Refresh Token的生成、验证、刷新和撤销功能
 *
 * @author     myself
 * @date       2025/08/09
 *
 *****************************************************************************/

#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <algorithm>

#include "../../common/utils/global.hpp"
#include "../../common/database/redis_mgr.hpp"

namespace im::gateway {

using json = nlohmann::json;
using im::db::RedisManager;

/**
 * @brief 生成随机字符串，用于创建Refresh Token
 * @param length 生成字符串的长度，默认为32位
 * @return 随机生成的字符串
 */
static std::string rt_generate(size_t length = 32) {
        const std::string chars =
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789-_";
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, chars.size() - 1);
        
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        return result;
    }


/**
 * @brief 根据平台字符串获取对应的平台类型枚举值
 * @param platform_str 平台字符串标识，如 "web", "android", "ios" 等
 * @return 对应的PlatformType枚举值
 */
static PlatformType get_platform_type(const std::string& platform_str) {
    if (platform_str == "web" || platform_str == "Web" || platform_str == "WEB") {
        return PlatformType::WEB;
    } else if (platform_str == "miniapp" || platform_str == "mini_app" ||
               platform_str == "Miniapp" || platform_str == "MiniApp" ||
               platform_str == "MINIAPP") {
        return PlatformType::MiniApp;
    } else if (platform_str == "ios" || platform_str == "iOS" || platform_str == "IOS") {
        return PlatformType::IOS;
    } else if (platform_str == "android" || platform_str == "Android" ||
               platform_str == "ANDROID") {
        return PlatformType::ANDROID;
    } else if (platform_str == "desktop" || platform_str == "Desktop" ||
               platform_str == "DESKTOP") {
        return PlatformType::DESKTOP;
    } else if (platform_str == "mobile" || platform_str == "Mobile" || platform_str == "MOBILE") {
        return PlatformType::MOBILE;
    } else {
        return PlatformType::UNKNOWN;
    }
}



/**
 * @brief 用户Token信息结构体，包含认证Token的相关用户信息
 */
struct UserTokenInfo {
    std::string user_id;   ///< 用户唯一标识
    std::string username;  ///< 用户名
    std::string device_id; ///< 设备唯一标识
    std::string platform;  ///< 平台标识，例如 "web", "mobile", "desktop"
    std::chrono::system_clock::time_point create_time; ///< Token创建时间
    std::chrono::system_clock::time_point expire_time; ///< Token过期时间
};


/**
 * @brief 刷新Token配置结构体，定义Refresh Token的刷新策略
 */
struct RefreshConfig {
    float refresh_precentage;   ///< 自动刷新率，当剩余有效期比例低于此值时触发刷新
    bool auto_refresh_enabled;  ///< 是否启用自动刷新功能
    bool background_refresh;    ///< 是否支持后台刷新
    int max_retry_count;        ///< 最大重试次数
};

/**
 * @brief Token时间配置结构体，定义Access Token和Refresh Token的过期时间
 */
struct TokenTimeConfig {
    int access_token_expire_seconds;  ///< Access Token过期时间（秒）
    int refresh_token_expire_seconds; ///< Refresh Token过期时间（秒）
};

/**
 * @brief 平台Token配置结构体，定义特定平台的Token配置策略
 */
struct PlatformTokenConfig {
    PlatformType platform;           ///< 平台类型
    RefreshConfig refresh_config;    ///< 刷新配置
    TokenTimeConfig token_time_config; ///< Token时间配置
    bool enable_multi_device;        ///< 是否允许多设备登录
};
/**
 * @brief Token操作结果结构体，封装Token生成或刷新操作的结果
 */
struct TokenResult {
    bool success;                 ///< 操作是否成功
    std::string new_access_token; ///< 新生成的Access Token
    std::string new_refresh_token;///< 新生成的Refresh Token
    std::string error_message;    ///< 错误信息（如果操作失败）
};


/**
 * @brief 平台Token策略类，负责管理不同平台的Token配置策略
 *        从配置文件中读取各平台的Token配置，并提供查询接口
 */
class PlatformTokenStrategy {
public:
    /**
     * @brief 构造函数，从指定配置文件路径加载平台Token配置
     * @param config_path 配置文件路径
     */
    PlatformTokenStrategy(std::string config_path);

    /**
     * @brief 获取指定平台的Token配置
     * @param platform 平台字符串标识
     * @return 对应平台的Token配置引用
     */
    const PlatformTokenConfig& get_platform_token_config(const std::string& platform);

private:
    std::unordered_map<PlatformType, PlatformTokenConfig> platform_token_configs_; ///< 平台Token配置映射表
};


/**
 * @brief 多平台认证管理器类，实现基于JWT的双Token认证机制
 *        支持Access Token和Refresh Token的完整生命周期管理
 */
class MultiPlatformAuthManager {
public:
    /**
     * @brief 构造函数，初始化认证管理器
     * @param secret_key JWT签名密钥
     * @param config_path 平台配置文件路径
     */
    MultiPlatformAuthManager(std::string secret_key, const std::string& config_path);
    
    MultiPlatformAuthManager(const std::string& config_path);



    /**
     * @brief 生成Access Token
     * @param user_id 用户ID
     * @param username 用户名
     * @param device_id 设备ID
     * @param platform 平台标识
     * @param expire_seconds 过期时间（秒），如果为0则使用平台默认配置
     * @return 生成的Access Token字符串
     */
    std::string generate_access_token(const std::string& user_id, const std::string& username,
                                      const std::string& device_id, const std::string& platform,
                                      int expire_seconds = 0);

    /**
     * @brief 生成Refresh Token
     * @param user_id 用户ID
     * @param username 用户名
     * @param device_id 设备ID
     * @param platform 平台标识
     * @param expire_seconds 过期时间（秒），如果为0则使用平台默认配置
     * @return 生成的Refresh Token字符串
     */
    std::string generate_refresh_token(const std::string& user_id, const std::string& username,
                                       const std::string& device_id, const std::string& platform,
                                       int expire_seconds = 0);

    /**
     * @brief 同时生成Access Token和Refresh Token
     * @param user_id 用户ID
     * @param username 用户名
     * @param device_id 设备ID
     * @param platform 平台标识
     * @return TokenResult结构体，包含生成的Token和操作结果
     */
    TokenResult generate_tokens(const std::string& user_id, const std::string& username,
                                const std::string& device_id, const std::string& platform);

    /**
     * @brief 使用Refresh Token刷新Access Token
     * @param refresh_token Refresh Token字符串
     * @param device_id 设备ID，用于验证Token与设备的绑定关系
     * @return TokenResult结构体，包含新生成的Access Token和操作结果
     */
    TokenResult refresh_access_token(const std::string& refresh_token, std::string& device_id);

    /**
     * @brief 验证Access Token的有效性
     * @param access_token Access Token字符串
     * @param user_info 用户信息结构体，用于返回解析出的用户信息
     * @return Token验证是否成功
     */
    bool verify_access_token(const std::string& access_token, UserTokenInfo& user_info);

    /**
     * @brief 验证Refresh Token的有效性
     * @param refresh_token Refresh Token字符串
     * @param device_id 设备ID，用于验证Token与设备的绑定关系
     * @param user_info 用户信息结构体，用于返回解析出的用户信息
     * @return Token验证是否成功
     */
    bool verify_refresh_token(const std::string& refresh_token, std::string& device_id, UserTokenInfo& user_info);

    /**
     * @brief 检查Token是否已被撤销
     * @param token Token字符串
     * @return Token是否已被撤销
     */
    bool is_token_revoked(const std::string& token);

    /**
     * @brief 撤销Access Token
     * @param token Access Token字符串
     * @return 撤销操作是否成功
     */
    bool revoke_token(const std::string& token);

    /**
     * @brief 取消撤销Access Token
     * @param token Access Token字符串
     * @return 取消撤销操作是否成功
     */
    bool unrevoke_token(const std::string& token);

    /**
     * @brief 撤销Refresh Token
     * @param refresh_token Refresh Token字符串
     * @return 撤销操作是否成功
     */
    bool revoke_refresh_token(const std::string& refresh_token);

    /**
     * @brief 取消撤销Refresh Token
     * @param refresh_token Refresh Token字符串
     * @return 取消撤销操作是否成功
     */
    bool unrevoke_refresh_token(const std::string& refresh_token);

    /**
     * @brief 删除Refresh Token
     * @param refresh_token Refresh Token字符串
     * @return 删除操作是否成功
     */
    bool del_refresh_token(const std::string& refresh_token);
    
private:
    /**
     * @brief 判断是否需要轮换Refresh Token
     * @param refresh_token Refresh Token字符串
     * @return 是否需要轮换
     */
    bool should_rotate_refresh_token(const std::string& refresh_token);

    /**
     * @brief 从Token中提取JWT ID (jti)
     * @param token Token字符串
     * @return JWT ID字符串
     */
    std::string extract_jti(const std::string& token) const;

private:
    std::string secret_key_;                     ///< JWT签名密钥
    PlatformTokenStrategy platform_token_strategy_; ///< 平台Token策略管理器
};


}  // namespace im::gateway



#endif  // MULTI_PLATFORM_AUTH_HPP