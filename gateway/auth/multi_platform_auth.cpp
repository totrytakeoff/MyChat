#include <uuid/uuid.h>
#include "../../common/utils/config_mgr.hpp"
#include "../../common/utils/log_manager.hpp"
#include "multi_platform_auth.hpp"


namespace im::gateway {
using im::utils::ConfigManager;


PlatformTokenStrategy::PlatformTokenStrategy(std::string config_path) {
    try {
        auto config = ConfigManager(config_path);

        // 直接读取各个平台的配置项
        std::vector<std::string> platforms = {"web", "android", "ios", "desktop", "miniapp", "mobile","unkown"};
        
        for (const auto& platform : platforms) {
            PlatformTokenConfig platform_token_config;
            RefreshConfig refresh_config;
            TokenTimeConfig token_time_config;

            std::string prefix = "PlatformTokenStrategy." + platform + ".";
            
            refresh_config.auto_refresh_enabled = config.get<bool>(prefix + "auto_refresh_enabled", true);
            refresh_config.background_refresh = config.get<bool>(prefix + "background_refresh", true);
            refresh_config.refresh_precentage = config.get<float>(prefix + "refresh_precentage", 0.3f);
            refresh_config.max_retry_count = config.get<int>(prefix + "max_retry_count", 1);

            token_time_config.access_token_expire_seconds =
                    config.get<int>(prefix + "access_token_expire_seconds", 0);
            token_time_config.refresh_token_expire_seconds =
                    config.get<int>(prefix + "refresh_token_expire_seconds", 0);

            platform_token_config.platform = get_platform_type(platform);
            platform_token_config.enable_multi_device =
                    config.get<bool>(prefix + "enable_multi_device", false);
            platform_token_config.refresh_config = refresh_config;
            platform_token_config.token_time_config = token_time_config;

            platform_token_configs_[platform_token_config.platform] = platform_token_config;
        }

    } catch (std::exception& e) {
        std::cout << "PlatformTokenStrategy config error: " << e.what() << std::endl;
        throw;
    }
}


const PlatformTokenConfig& PlatformTokenStrategy::get_platform_token_config(
        const std::string& platform) {
    return platform_token_configs_[get_platform_type(platform)];
}



MultiPlatformAuthManager::MultiPlatformAuthManager(std::string secret_key,
                                                   const std::string& config_path)
        : secret_key_(std::move(secret_key)), platform_token_strategy_(config_path) {}

std::string MultiPlatformAuthManager::generate_access_token(const std::string& user_id,
                                                            const std::string& username,
                                                            const std::string& device_id,
                                                            const std::string& platform,
                                                            int expire_seconds) {
    try {
        auto now = std::chrono::system_clock::now();

        auto time = platform_token_strategy_.get_platform_token_config(platform)
                            .token_time_config.access_token_expire_seconds;


        auto expire_time = now + std::chrono::seconds(expire_seconds > 0 ? expire_seconds : time);

        // 生成唯一的JWT ID
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);
        std::string jti(uuid_str);
        // 创建JWT Token
        auto token =
                jwt::create()
                        .set_issuer("mychat-gateway")  // 签发者
                        .set_issued_at(now)
                        .set_expires_at(expire_time)
                        .set_id(jti)
                        .set_subject(user_id)           // 主题（用户ID）
                        .set_audience("mychat-client")  // 接收者
                        .set_payload_claim("username", jwt::claim(username))           // 用户名
                        .set_payload_claim("device_id", jwt::claim(device_id))         // 设备
                        .set_payload_claim("platform", jwt::claim(platform))           // 平台
                        .set_payload_claim("type", jwt::claim(std::string("access")))  // Token类型
                        .sign(jwt::algorithm::hs256{secret_key_});                     // 签名
        return token;
    } catch (...) {
        // 生成失败
        return "";
    }
}

std::string MultiPlatformAuthManager::generate_refresh_token(const std::string& user_id,
                                                             const std::string& username,
                                                             const std::string& device_id,
                                                             const std::string& platform,
                                                             int expire_seconds) {
    try {
        auto now = std::chrono::system_clock::now();


        auto times = platform_token_strategy_.get_platform_token_config(platform)
                             .token_time_config.refresh_token_expire_seconds;

        auto expire_time = now + std::chrono::seconds(expire_seconds > 0 ? expire_seconds : times);

        auto rt = rt_generate();

        json rtMeta = {{"user_id", user_id},
                       {"username", username},
                       {"device_id", device_id},
                       {"platform", platform},
                       {"expire_time", expire_time.time_since_epoch().count()},
                       {"create_time", now.time_since_epoch().count()},
                       {"jti", rt},
                       {"type", "refresh"},
                       {"revoked", false},
                       {"last_used", now.time_since_epoch().count()}};

                       
                       
        return rt;
    }



    catch (...) {
        // 生成失败
        return "";
    }
}

TokenResult MultiPlatformAuthManager::generate_tokens(const std::string& user_id,
                                                      const std::string& username,
                                                      const std::string& device_id,
                                                      const std::string& platform) {
    TokenResult result;
    try {
        result.new_access_token = generate_access_token(user_id, username, device_id, platform);
        result.new_refresh_token = generate_refresh_token(user_id, username, device_id, platform);
        result.sucess = true;
    } catch (const std::exception& e) {
        result.sucess = false;
        result.error_message = e.what() || "Unknown error";
        result.new_access_token = "";
        result.new_refresh_token = "";
    }
    return result;
}

TokenResult MultiPlatformAuthManager::refresh_access_token(const std::string& refresh_token) {
    TokenResult result;
    try {
        UserTokenInfo user_info;
        if (!verify_refresh_token(refresh_token, user_info)) {
            result.sucess = false;
            result.error_message = "Invalid refresh token";
            return result;
        }

        // 检查是否需要轮换刷新令牌
        if (should_rotate_refresh_token(refresh_token)) {
            // 如果需要轮换，撤销旧的刷新令牌
            revoke_token(refresh_token);
            // 生成新的刷新令牌
            result.new_refresh_token = generate_refresh_token(
                    user_info.user_id, user_info.username, user_info.device_id, user_info.platform);
        }

        // 生成新的访问令牌
        result.new_access_token = generate_access_token(user_info.user_id, user_info.username,
                                                        user_info.device_id, user_info.platform);


        result.sucess = true;

        return result;
    } catch (const std::exception& e) {
        result.sucess = false;
        result.error_message = e.what() || "Unknown error";
        result.new_access_token = "";
        result.new_refresh_token = "";
    }
}

bool MultiPlatformAuthManager::verify_access_token(const std::string& access_token,
                                                   UserTokenInfo& user_info) {
    try {
        // 创建验证器
        auto verifier = jwt::verify()
                                .allow_algorithm(jwt::algorithm::hs256{secret_key_})
                                .with_issuer("mychat-gateway")
                                .with_audience("mychat-client");

        // 解码和验证Token
        auto decoded = jwt::decode(access_token);
        verifier.verify(decoded);

        // 检查是否在黑名单中
        if (is_token_revoked(access_token)) {
            return false;
        }

        // 提取用户信息
        user_info.user_id = decoded.get_subject();
        user_info.username = decoded.get_payload_claim("username").as_string();
        user_info.device_id = decoded.get_payload_claim("device_id").as_string();
        user_info.platform = decoded.get_payload_claim("platform").as_string();
        user_info.create_time = decoded.get_issued_at();
        user_info.expire_time = decoded.get_expires_at();

        return true;

    } catch (const std::exception& e) {
        // 验证失败（Token无效、过期等）
        return false;
    }
}

bool MultiPlatformAuthManager::verify_refresh_token(const std::string& refresh_token,
                                                    UserTokenInfo& user_info) {
    return false;
}

void MultiPlatformAuthManager::revoke_token(const std::string& token) {
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

void MultiPlatformAuthManager::unrevoke_token(const std::string& token) {
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

bool MultiPlatformAuthManager::is_token_revoked(const std::string& token) {
    try {
        std::string jti = extract_jti(token);
        if (jti.empty()) return false;

        std::lock_guard<std::mutex> lock(mutex_);
        return revoked_tokens_.find(jti) != revoked_tokens_.end();
    } catch (...) {
        return true;  // 出错时认为已撤销
    }
}

bool MultiPlatformAuthManager::should_rotate_refresh_token(const std::string& refresh_token) {
    try {
        auto decoded = jwt::decode(refresh_token);
        auto issued_at = decoded.get_issued_at();
        auto expires_at = decoded.get_expires_at();
        auto now = std::chrono::system_clock::now();

        return (expires_at - now) / (expires_at - issued_at) <
               platform_token_strategy_
                       .get_platform_token_config(decoded.get_payload_claim("platform").as_string())
                       .refresh_config.refresh_precentage;



    } catch (const std::exception& e) {
        std::cerr << "Error checking refresh token rotation: " << e.what() << std::endl;
        return false;
    }

    return false;
}

std::string MultiPlatformAuthManager::extract_jti(const std::string& token) const {
    try {
        auto decoded = jwt::decode(token);
        return decoded.get_id();
    } catch (...) {
        return "";
    }
}

}  // namespace im::gateway