#ifndef MULTI_PLATFORM_AUTH_HPP
#define MULTI_PLATFORM_AUTH_HPP

/******************************************************************************
 *
 * @file       multi_platform_auth.hpp
 * @brief      多平台认证管理器 负责跨平台用户认证和权限验证,rt at 双token 管理
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


static const PlatformType get_platform_type(const std::string& platform_str) {
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



struct UserTokenInfo {
    std::string user_id;   // 用户ID
    std::string username;  // 用户名
    std::string device_id;
    std::string platform;  // 平台标识，例如 "web", "mobile", "desktop"
    std::chrono::system_clock::time_point create_time;
    std::chrono::system_clock::time_point expire_time;
};


struct RefreshConfig {
    float refresh_precentage;   // 自动刷新率
    bool auto_refresh_enabled;  // 是否启用自动刷新
    bool background_refresh;    // 是否支持后台刷新
    int max_retry_count;        // 最大重试次数
};

struct TokenTimeConfig {
    int access_token_expire_seconds;
    int refresh_token_expire_seconds;
};

struct PlatformTokenConfig {
    PlatformType platform;
    RefreshConfig refresh_config;
    TokenTimeConfig token_time_config;
    bool enable_multi_device;  // 是否允许多设备登录
};
struct TokenResult {
    bool sucess;
    std::string new_access_token;
    std::string new_refresh_token;
    std::string error_message;
};


class PlatformTokenStrategy;


class MultiPlatformAuthManager {
public:
    MultiPlatformAuthManager(std::string secret_key, const std::string& config_path);



    std::string generate_access_token(const std::string& user_id, const std::string& username,
                                      const std::string& device_id, const std::string& platform,
                                      int expire_seconds = 0);

    std::string generate_refresh_token(const std::string& user_id, const std::string& username,
                                       const std::string& device_id, const std::string& platform,
                                       int expire_seconds = 0);

    TokenResult generate_tokens(const std::string& user_id, const std::string& username,
                                const std::string& device_id, const std::string& platform);


    //刷新access_token需要 device_id进行校验,因为refresh_token与device强绑定
    TokenResult refresh_access_token(const std::string& refresh_token,std::string& device_id);

    bool verify_access_token(const std::string& access_token, UserTokenInfo& user_info);
    bool verify_refresh_token(const std::string& refresh_token,std::string& device_id, UserTokenInfo& user_info);

    bool is_token_revoked(const std::string& token);

    bool revoke_token(const std::string& token);
    bool unrevoke_token(const std::string& token);

    bool revoke_refresh_token(const std::string& refresh_token);
    bool unrevoke_refresh_token(const std::string& refresh_token);

    bool del_refresh_token(const std::string& refresh_token);
    
private:
    bool should_rotate_refresh_token(const std::string& refresh_token);

    std::string extract_jti(const std::string& token) const;

private:
    std::string secret_key_;

    // mutable std::mutex mutex_;
    // std::unordered_set<std::string> revoked_tokens_; //暂时使用哈希表，后续可使用Redis等分布式存储
    PlatformTokenStrategy platform_token_strategy_;
};



class PlatformTokenStrategy {
public:
    PlatformTokenStrategy(std::string config_path);

    const PlatformTokenConfig& get_platform_token_config(const std::string& platform);

private:
    std::unordered_map<PlatformType, PlatformTokenConfig> platform_token_configs_;
};


}  // namespace im::gateway



#endif  // MULTI_PLATFORM_AUTH_HPP