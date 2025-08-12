#ifndef AUTH_MGR_HPP
#define AUTH_MGR_HPP

/******************************************************************************
 *
 * @file       aurh_mgr.hpp
 * @brief      认证管理器 负责用户认证和权限验证
 *
 * @author     myself
 * @date       2025/08/07
 *
 *****************************************************************************/


#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <jwt-cpp/jwt.h>



namespace im {
namespace gateway {

struct UserTokenInfo {
    std::string user_id;   // 用户ID
    std::string username;  // 用户名
    std::string device_id;
    std::chrono::system_clock::time_point create_time;
    std::chrono::system_clock::time_point expire_time;
    // std::string ip_address;  // 生成token时的IP地址
    // std::string user_agent;  // 生成token时的User-Agent
};

class AuthManager {
public:
    AuthManager(std::string secret_key, int expire_seconds = 86400);  // 默认过期时间为24小时

    std::string generate_token(const std::string& user_id, const std::string& username,
                               const std::string& device_id = "");

    bool verify_token(const std::string& token, UserTokenInfo& user_info);

    void revoke_token(const std::string& token);

    void unrevoke_token(const std::string& token);
    bool is_token_revoked(const std::string& token) {
        return revoked_tokens_.find(token) != revoked_tokens_.end();
    }


private:
    std::string extract_jti(const std::string& token) const;

private:
    std::string secret_key_;
    int expire_seconds_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> revoked_tokens_;  // 存储已撤销的token(jti)
};



}  // namespace gateway
}  // namespace im



#endif  // AURH_MGR_HPP