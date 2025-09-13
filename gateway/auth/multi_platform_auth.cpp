#include <uuid/uuid.h>
#include "../../common/database/redis_mgr.hpp"
#include "../../common/utils/config_mgr.hpp"
#include "../../common/utils/log_manager.hpp"
#include "multi_platform_auth.hpp"


namespace im::gateway {
using im::db::RedisConfig;
using im::db::RedisManager;
using im::utils::ConfigManager;
using im::utils::LogManager;

PlatformTokenStrategy::PlatformTokenStrategy(std::string config_path) {
    try {
        auto config = ConfigManager(config_path);

        // 直接读取各个平台的配置项
        std::vector<std::string> platforms = {"web",     "android", "ios",   "desktop",
                                              "miniapp", "mobile",  "unkown"};

        for (const auto& platform : platforms) {
            PlatformTokenConfig platform_token_config;
            RefreshConfig refresh_config;
            TokenTimeConfig token_time_config;

            std::string prefix = "PlatformTokenStrategy." + platform + ".";

            refresh_config.auto_refresh_enabled =
                    config.get<bool>(prefix + "auto_refresh_enabled", true);
            refresh_config.background_refresh =
                    config.get<bool>(prefix + "background_refresh", true);
            refresh_config.refresh_precentage =
                    config.get<float>(prefix + "refresh_precentage", 0.3f);
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
        : secret_key_(std::move(secret_key)), platform_token_strategy_(config_path) {
    try {
        if (!RedisManager::GetInstance().is_healthy()) {
            RedisManager::GetInstance().initialize(config_path);
        }
        if (!RedisManager::GetInstance().is_healthy()) {
            LogManager::GetLogger("auth_mgr")->error("redis is not connected or not healthy!");
            throw;
        }

    } catch (std::exception& e) {
        LogManager::GetLogger("auth_mgr")->error("auth_mgr initialize failed!");
        throw;
    }
}

MultiPlatformAuthManager::MultiPlatformAuthManager(const std::string& config_path)
        : platform_token_strategy_(config_path) {
    ConfigManager config(config_path);
    try {
        secret_key_ = config.get<std::string>("secret_key", "default_secret_key");

        if (!RedisManager::GetInstance().is_healthy()) {
            RedisManager::GetInstance().initialize(config_path);
        }
        if (!RedisManager::GetInstance().is_healthy()) {
            LogManager::GetLogger("auth_mgr")->error("redis is not connected or not healthy!");
            throw;
        }

    } catch (std::exception& e) {
        LogManager::GetLogger("auth_mgr")->error("auth_mgr initialize failed!");
        throw;
    }
}

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
        LogManager::GetLogger("auth_mgr")->error("generate access token failed!");
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

        RedisManager::GetInstance().execute([&](auto& redis) {
            redis.hset("refresh_tokens", rt, rtMeta.dump());
            redis.expire("refresh_tokens", times);
            redis.sadd("user:" + user_id + ":rt", rt);
            redis.expire("user:" + user_id + ":rt", times);
        });

        return rt;
    }



    catch (...) {
        // 生成失败
        LogManager::GetLogger("auth_mgr")->error("generate refresh token failed!");
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
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what() ? e.what() : "Unknown error";
        result.new_access_token = "";
        result.new_refresh_token = "";
    }
    return result;
}

TokenResult MultiPlatformAuthManager::refresh_access_token(const std::string& refresh_token,
                                                           std::string& device_id) {
    TokenResult result;
    try {
        UserTokenInfo user_info;
        if (!verify_refresh_token(refresh_token, device_id, user_info)) {
            result.success = false;
            result.error_message = "Invalid refresh token";
            return result;
        }

        // 检查是否需要轮换刷新令牌
        if (should_rotate_refresh_token(refresh_token)) {
            // 如果需要轮换，撤销旧的刷新令牌
            // revoke_token(refresh_token);
            // 生成新的刷新令牌
            result.new_refresh_token = generate_refresh_token(
                    user_info.user_id, user_info.username, user_info.device_id, user_info.platform);
        }

        // 生成新的访问令牌
        result.new_access_token = generate_access_token(user_info.user_id, user_info.username,
                                                        user_info.device_id, user_info.platform);


        result.success = true;

        return result;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what() ? e.what() : "Unknown error";
        result.new_access_token = "";
        result.new_refresh_token = "";
        return result;
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
        LogManager::GetLogger("auth_mgr")->error("verify_access_token error: {}", e.what());
        return false;
    }
}

bool MultiPlatformAuthManager::verify_access_token(const std::string& access_token,
                                                   const std::string& device_id) {
    try {
        if (access_token.empty()) {
            throw std::runtime_error("Empty token");
        }

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
            throw std::runtime_error("Token is revoked");
        }

        // 验证 device_id 是否匹配
        if (device_id != decoded.get_payload_claim("device_id").as_string()) {
            throw std::runtime_error("Device ID does not match");
        }


        return true;

    } catch (const std::exception& e) {
        LogManager::GetLogger("auth_mgr")->error("verify_access_token error: {}", e.what());
        return false;
    }
}

bool MultiPlatformAuthManager::verify_refresh_token(const std::string& refresh_token,
                                                    std::string& device_id,
                                                    UserTokenInfo& user_info) {
    try {
        if (refresh_token.empty()) {
            throw std::runtime_error("Empty token");
        }

        auto meta = RedisManager::GetInstance().execute(
                [&](auto& redis) { return redis.hget("refresh_tokens", refresh_token); });

        // 检查meta是否为空
        if (!meta) {
            return false;
        }

        json decode = json::parse(*meta);
        if (!decode.is_null()) {
            if (decode["device_id"] != device_id) {
                // rt和device_id不匹配，属于严重安全问题,后续应该采取如吊销rt等措施
                revoke_refresh_token(refresh_token);
                throw std::runtime_error("Device ID does not match");
            }
            if (decode["revoked"]) {
                throw std::runtime_error("Token is revoked");
            }

            user_info.device_id = decode["device_id"];
            user_info.platform = decode["platform"];
            user_info.user_id = decode["user_id"];
            user_info.username = decode["username"];
            auto create_time_ns = decode["create_time"].get<int64_t>();
            auto expire_time_ns = decode["expire_time"].get<int64_t>();
            user_info.create_time =
                    std::chrono::system_clock::time_point(std::chrono::nanoseconds(create_time_ns));
            user_info.expire_time =
                    std::chrono::system_clock::time_point(std::chrono::nanoseconds(expire_time_ns));
            return true;
        }
        throw std::runtime_error("Invalid refresh token");
    } catch (const std::exception& e) {
        // 验证失败（Token无效、过期等）
        LogManager::GetLogger("auth_mgr")->error("verify_refresh_token error: {}", e.what());
        return false;
    }
}

bool MultiPlatformAuthManager::revoke_token(const std::string& token) {
    try {
        std::string jti = extract_jti(token);
        if (!jti.empty()) {
            auto conn = RedisManager::GetInstance().get_connection();
            conn->sadd("revoked_access_tokens", jti);
            return true;
        }

        return false;
    } catch (...) {
        // 忽略错误
        return false;
    }
}

bool MultiPlatformAuthManager::unrevoke_token(const std::string& token) {
    try {
        std::string jti = extract_jti(token);
        if (!jti.empty()) {
            auto conn = RedisManager::GetInstance().get_connection();
            conn->srem("revoked_access_tokens", jti);
            return true;
        }
        return false;
    } catch (...) {
        // 忽略错误
        return false;
    }
}

bool MultiPlatformAuthManager::revoke_refresh_token(const std::string& refresh_token) {
    try {
        RedisManager::GetInstance().execute([&](auto& redis) {
            auto meta = redis.hget("refresh_tokens", refresh_token);
            // 检查meta是否为空
            if (!meta) {
                return;
            }
            json decode = json::parse(*meta);
            decode["revoked"] = true;
            redis.hset("refresh_tokens", refresh_token, decode.dump());
        });

        return true;
    } catch (...) {
        // 忽略错误
        return false;
    }
}

bool MultiPlatformAuthManager::unrevoke_refresh_token(const std::string& refresh_token) {
    try {
        RedisManager::GetInstance().execute([&](auto& redis) {
            auto meta = redis.hget("refresh_tokens", refresh_token);
            // 检查meta是否为空
            if (!meta) {
                return;
            }
            json decode = json::parse(*meta);
            decode["revoked"] = false;
            redis.hset("refresh_tokens", refresh_token, decode.dump());
        });

        return true;
    } catch (...) {
        // 忽略错误
        return false;
    }
}

bool MultiPlatformAuthManager::del_refresh_token(const std::string& refresh_token) {
    try {
        RedisManager::GetInstance().execute(
                [&](auto& redis) { redis.hdel("refresh_tokens", refresh_token); });

        return true;
    } catch (...) {
        // 忽略错误
        return false;
    }
}

bool MultiPlatformAuthManager::is_token_revoked(const std::string& token) {
    try {
        std::string jti = extract_jti(token);
        if (jti.empty()) return false;
        auto conn = RedisManager::GetInstance().get_connection();
        return conn->sismember("revoked_access_tokens", jti);

    } catch (...) {
        return true;  // 出错时认为已撤销
    }
}

bool MultiPlatformAuthManager::should_rotate_refresh_token(const std::string& refresh_token) {
    try {
        auto conn = RedisManager::GetInstance().get_connection();
        auto meta = conn->hget("refresh_tokens", refresh_token);
        // 检查meta是否为空
        if (!meta) {
            return false;
        }
        json decode = json::parse(*meta);

        auto issued_at = decode["create_time"].get<int64_t>();
        auto expires_at = decode["expire_time"].get<int64_t>();
        auto now = std::chrono::system_clock::now().time_since_epoch().count();

        return (double)(expires_at - now) / (double)(expires_at - issued_at) <
               platform_token_strategy_
                       .get_platform_token_config(decode["platform"].get<std::string>())
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