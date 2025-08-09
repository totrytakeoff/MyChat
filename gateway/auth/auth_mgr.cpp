#include <uuid/uuid.h>
#include "auth_mgr.hpp"

namespace im::gateway {
AuthManager::AuthManager(std::string secret_key, int expire_seconds)
        : secret_key_(secret_key), expire_seconds_(expire_seconds) {}

std::string AuthManager::generate_token(const std::string& user_id, const std::string& username,
                                        const std::string& device_id) {
    try {
        // 生成唯一的JWT ID
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);
        std::string jti(uuid_str);

        // 计算过期时间
        auto now = std::chrono::system_clock::now();
        auto expire_time = now + std::chrono::seconds(expire_seconds_);
        // 创建JWT Token
        auto token = jwt::create()
                             .set_issuer("mychat-gateway")   // 签发者
                             .set_subject(user_id)           // 主题（用户ID）
                             .set_audience("mychat-client")  // 接收者
                             .set_issued_at(now)             // 签发时间
                             .set_expires_at(expire_time)    // 过期时间
                             .set_id(jti)                    // JWT唯一ID
                             .set_payload_claim("username", jwt::claim(username))    // 用户名
                             .set_payload_claim("device_id", jwt::claim(device_id))  // 设备ID
                             .sign(jwt::algorithm::hs256{secret_key_});              // 签名

        return token;

    } catch (const std::exception& e) {
        // 生成失败
        return "";
    }
}

bool AuthManager::verify_token(const std::string& token, UserTokenInfo& user_info) {
    try {
        // 创建验证器
        auto verifier = jwt::verify()
                                .allow_algorithm(jwt::algorithm::hs256{secret_key_})
                                .with_issuer("mychat-gateway")
                                .with_audience("mychat-client");

        // 解码和验证Token
        auto decoded = jwt::decode(token);
        verifier.verify(decoded);

        // 检查是否在黑名单中
        if (is_token_revoked(token)) {
            return false;
        }

        // 提取用户信息
        user_info.user_id = decoded.get_subject();
        user_info.username = decoded.get_payload_claim("username").as_string();
        user_info.device_id = decoded.get_payload_claim("device_id").as_string();
        user_info.create_time = decoded.get_issued_at();

        return true;

    } catch (const std::exception& e) {
        // 验证失败（Token无效、过期等）
        return false;
    }
}

void AuthManager::revoke_token(const std::string& token) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string jti = extract_jti(token);
        if (!jti.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            revoked_tokens_.insert(jti);
        }
    } catch (...) {
        // 忽略错误
    }
}

void AuthManager::unrevoke_token(const std::string& token) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string jti = extract_jti(token);
        if (!jti.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            revoked_tokens_.erase(jti);
        }
    } catch (...) {
        // 忽略错误
    }
}

bool AuthManager::is_token_revoked(const std::string& token) {
    try {
        std::string jti = extract_jti(token);
        if (jti.empty()) return false;

        std::lock_guard<std::mutex> lock(mutex_);
        return revoked_tokens_.find(jti) != revoked_tokens_.end();
    } catch (...) {
        return true;  // 出错时认为已撤销
    }
}

std::string AuthManager::extract_jti(const std::string& token) const {
    try {
        auto decoded = jwt::decode(token);
        return decoded.get_id();
    } catch (...) {
        return "";
    }
}


}  // namespace im::gateway