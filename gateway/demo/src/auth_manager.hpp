#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include "common/utils/log_manager.hpp"
#include "common/utils/json.hpp"

using json = nlohmann::json;

/**
 * @brief 用户Token信息
 */
struct UserTokenInfo {
    std::string user_id;                                    // 用户ID
    std::string username;                                   // 用户名
    std::string device_id;                                  // 设备ID
    std::chrono::system_clock::time_point create_time;     // Token创建时间
    std::chrono::system_clock::time_point expire_time;     // Token过期时间
    std::chrono::system_clock::time_point last_access;     // 最后访问时间
    
    UserTokenInfo() = default;
    UserTokenInfo(const std::string& uid, const std::string& uname, 
                 const std::string& dev_id, int expire_seconds)
        : user_id(uid), username(uname), device_id(dev_id) {
        auto now = std::chrono::system_clock::now();
        create_time = now;
        expire_time = now + std::chrono::seconds(expire_seconds);
        last_access = now;
    }
    
    bool IsExpired() const {
        return std::chrono::system_clock::now() > expire_time;
    }
    
    void UpdateAccess() {
        last_access = std::chrono::system_clock::now();
    }
    
    // 转换为JSON（用于缓存）
    json ToJson() const {
        json j;
        j["user_id"] = user_id;
        j["username"] = username;
        j["device_id"] = device_id;
        j["create_time"] = std::chrono::duration_cast<std::chrono::seconds>(
            create_time.time_since_epoch()).count();
        j["expire_time"] = std::chrono::duration_cast<std::chrono::seconds>(
            expire_time.time_since_epoch()).count();
        j["last_access"] = std::chrono::duration_cast<std::chrono::seconds>(
            last_access.time_since_epoch()).count();
        return j;
    }
    
    // 从JSON恢复
    static UserTokenInfo FromJson(const json& j) {
        UserTokenInfo info;
        if (j.contains("user_id")) info.user_id = j["user_id"];
        if (j.contains("username")) info.username = j["username"];
        if (j.contains("device_id")) info.device_id = j["device_id"];
        
        if (j.contains("create_time")) {
            info.create_time = std::chrono::system_clock::from_time_t(j["create_time"]);
        }
        if (j.contains("expire_time")) {
            info.expire_time = std::chrono::system_clock::from_time_t(j["expire_time"]);
        }
        if (j.contains("last_access")) {
            info.last_access = std::chrono::system_clock::from_time_t(j["last_access"]);
        }
        
        return info;
    }
};

/**
 * @brief 认证管理器 - 负责用户认证和Token管理
 * 
 * 主要功能：
 * 1. Token生成、验证和管理
 * 2. 用户登录状态维护
 * 3. Token过期清理
 * 4. 安全策略控制
 */
class AuthManager {
public:
    AuthManager();
    ~AuthManager();
    
    /**
     * @brief 初始化认证管理器
     * @param secret_key JWT密钥
     * @param expire_seconds Token过期时间（秒）
     * @return 是否初始化成功
     */
    bool Initialize(const std::string& secret_key, int expire_seconds = 86400);
    
    // ==================== Token 管理 ====================
    
    /**
     * @brief 生成用户Token
     * @param user_id 用户ID
     * @param username 用户名
     * @param device_id 设备ID
     * @return 生成的Token，失败返回空字符串
     */
    std::string GenerateToken(const std::string& user_id, 
                             const std::string& username,
                             const std::string& device_id = "");
    
    /**
     * @brief 验证Token的有效性
     * @param token Token字符串
     * @param user_id 输出参数：用户ID
     * @return 是否有效
     */
    bool VerifyToken(const std::string& token, std::string& user_id);
    
    /**
     * @brief 验证Token并获取用户信息
     * @param token Token字符串
     * @param user_info 输出参数：用户信息
     * @return 是否有效
     */
    bool VerifyTokenAndGetInfo(const std::string& token, UserTokenInfo& user_info);
    
    /**
     * @brief 刷新Token（延长过期时间）
     * @param token 原Token
     * @return 新Token，失败返回空字符串
     */
    std::string RefreshToken(const std::string& token);
    
    /**
     * @brief 撤销Token（用户登出）
     * @param token Token字符串
     * @return 是否撤销成功
     */
    bool RevokeToken(const std::string& token);
    
    /**
     * @brief 撤销用户的所有Token（强制下线）
     * @param user_id 用户ID
     * @return 撤销的Token数量
     */
    size_t RevokeUserTokens(const std::string& user_id);
    
    // ==================== 用户状态查询 ====================
    
    /**
     * @brief 检查用户是否已登录
     * @param user_id 用户ID
     * @return 是否已登录
     */
    bool IsUserLoggedIn(const std::string& user_id);
    
    /**
     * @brief 获取用户的活跃Token数量
     * @param user_id 用户ID
     * @return Token数量
     */
    size_t GetUserTokenCount(const std::string& user_id);
    
    /**
     * @brief 获取用户的所有设备信息
     * @param user_id 用户ID
     * @return 设备ID列表
     */
    std::vector<std::string> GetUserDevices(const std::string& user_id);
    
    // ==================== 安全策略 ====================
    
    /**
     * @brief 设置单用户最大Token数量（多端登录限制）
     * @param max_tokens 最大Token数
     */
    void SetMaxTokensPerUser(size_t max_tokens) { max_tokens_per_user_ = max_tokens; }
    
    /**
     * @brief 设置Token自动续期
     * @param auto_refresh 是否自动续期
     */
    void SetAutoRefresh(bool auto_refresh) { auto_refresh_ = auto_refresh; }
    
    /**
     * @brief 检查是否需要踢掉旧的Token
     * @param user_id 用户ID
     * @return 被踢掉的Token列表
     */
    std::vector<std::string> CheckAndEvictOldTokens(const std::string& user_id);
    
    // ==================== 统计和监控 ====================
    
    /**
     * @brief 获取当前活跃Token数量
     * @return Token数量
     */
    size_t GetActiveTokenCount() const;
    
    /**
     * @brief 获取登录用户数量
     * @return 用户数量
     */
    size_t GetLoggedInUserCount() const;
    
    /**
     * @brief 获取认证统计信息
     * @return 统计信息JSON字符串
     */
    std::string GetAuthStats() const;
    
    // ==================== Token 清理 ====================
    
    /**
     * @brief 清理过期Token
     * @return 清理的Token数量
     */
    size_t CleanupExpiredTokens();
    
    /**
     * @brief 启动定时清理任务
     */
    void StartCleanupTask();
    
    /**
     * @brief 停止定时清理任务
     */
    void StopCleanupTask();

private:
    // ==================== 内部方法 ====================
    
    /**
     * @brief 生成随机Token字符串
     * @param user_id 用户ID
     * @return Token字符串
     */
    std::string GenerateTokenString(const std::string& user_id);
    
    /**
     * @brief 验证Token格式
     * @param token Token字符串
     * @return 是否格式正确
     */
    bool ValidateTokenFormat(const std::string& token);
    
    /**
     * @brief 定时清理任务
     */
    void DoCleanup();
    
    /**
     * @brief 生成认证统计信息
     * @return 统计信息JSON
     */
    std::string GenerateAuthStats() const;

private:
    // 线程安全保护
    mutable std::mutex mutex_;
    
    // Token缓存：token -> user_info
    std::unordered_map<std::string, UserTokenInfo> token_cache_;
    
    // 用户Token映射：user_id -> token_set
    std::unordered_map<std::string, std::unordered_set<std::string>> user_tokens_map_;
    
    // 配置参数
    std::string secret_key_;                    // 密钥
    int token_expire_seconds_;                  // Token过期时间
    size_t max_tokens_per_user_;               // 每个用户最大Token数
    bool auto_refresh_;                         // 是否自动续期
    
    // 日志管理器
    std::shared_ptr<LogManager> log_mgr_;
    
    // 清理任务控制
    std::atomic<bool> cleanup_running_;
    std::thread cleanup_thread_;
    
    // 统计信息
    std::atomic<size_t> total_generated_tokens_;    // 总生成Token数
    std::atomic<size_t> total_verify_requests_;     // 总验证请求数
    std::atomic<size_t> successful_verifications_;  // 成功验证数
};