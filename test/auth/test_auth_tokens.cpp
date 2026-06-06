#include "../../common/database/redis/redis_mgr.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../../gateway/auth/multi_platform_auth.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <thread>
#include <vector>

namespace {

im::db::RedisConfig test_redis_config() {
    im::db::RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "mychat-dev-pass";
    config.db = 15;
    config.pool_size = 4;
    config.connect_timeout = 1000;
    config.socket_timeout = 1000;
    return config;
}

class AuthTokenTest : public ::testing::Test {
protected:
    static std::string config_path() {
        return (std::filesystem::path(MYCHAT_SOURCE_DIR) / "config/dev.json").string();
    }

    void SetUp() override {
        im::utils::LogManager::SetLogToConsole("auth_mgr");
        im::utils::LogManager::SetLogToConsole("redis_manager");

        auto& redis = im::db::redis_manager();
        redis.shutdown();
        ASSERT_TRUE(redis.initialize(test_redis_config()))
                << "Start Redis with `docker compose up -d redis` before running this test.";
        cleanup();

        auth_ = std::make_unique<im::gateway::MultiPlatformAuthManager>(
                "test_secret_key_for_auth_token_tests", config_path());
    }

    void TearDown() override {
        cleanup();
        auth_.reset();
        im::db::redis_manager().shutdown();
    }

    static void cleanup_keys_like(const std::string& pattern) {
        auto keys = im::db::redis_manager().execute(
                [&](auto& redis) { return redis.keys(pattern); });
        for (const auto& key : keys) {
            im::db::redis_manager().execute([&](auto& redis) { redis.del(key); });
        }
    }

    void cleanup() {
        im::db::redis_manager().safe_execute(
                [](auto& redis) {
                    redis.del("refresh_tokens");
                    redis.del("revoked_access_tokens");
                    redis.del("user:test-user:rt");
                    redis.del("user:test-user-2:rt");
                    redis.del("user:test-user:refresh_tokens");
                    redis.del("user:test-user-2:refresh_tokens");
                    return true;
                },
                false);
        cleanup_keys_like("refresh_token:*");
        cleanup_keys_like("revoked_access_token:*");
    }

    std::unique_ptr<im::gateway::MultiPlatformAuthManager> auth_;
    const std::string user_id_ = "test-user";
    const std::string username_ = "alice";
    const std::string device_id_ = "device-web-1";
    const std::string platform_ = "web";
};

}  // namespace

TEST_F(AuthTokenTest, GenerateAndVerifyAccessToken) {
    auto token = auth_->generate_access_token(user_id_, username_, device_id_, platform_, 60);
    ASSERT_FALSE(token.empty());

    im::gateway::UserTokenInfo user_info;
    ASSERT_TRUE(auth_->verify_access_token(token, user_info));
    EXPECT_EQ(user_info.user_id, user_id_);
    EXPECT_EQ(user_info.username, username_);
    EXPECT_EQ(user_info.device_id, device_id_);
    EXPECT_EQ(user_info.platform, platform_);
    EXPECT_TRUE(auth_->verify_access_token(token, device_id_));
    EXPECT_FALSE(auth_->verify_access_token(token, "wrong-device"));
}

TEST_F(AuthTokenTest, ExpiredAccessTokenIsRejected) {
    auto token = auth_->generate_access_token(user_id_, username_, device_id_, platform_, 1);
    ASSERT_FALSE(token.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    im::gateway::UserTokenInfo user_info;
    EXPECT_FALSE(auth_->verify_access_token(token, user_info));
}

TEST_F(AuthTokenTest, RefreshTokenLifecycle) {
    auto refresh_token =
            auth_->generate_refresh_token(user_id_, username_, device_id_, platform_, 60);
    ASSERT_FALSE(refresh_token.empty());

    im::gateway::UserTokenInfo user_info;
    auto matching_device = device_id_;
    ASSERT_TRUE(auth_->verify_refresh_token(refresh_token, matching_device, user_info));
    EXPECT_EQ(user_info.user_id, user_id_);
    EXPECT_EQ(user_info.username, username_);
    EXPECT_EQ(user_info.device_id, device_id_);
    EXPECT_EQ(user_info.platform, platform_);

    auto wrong_device = std::string("wrong-device");
    EXPECT_FALSE(auth_->verify_refresh_token(refresh_token, wrong_device, user_info));

    matching_device = device_id_;
    EXPECT_FALSE(auth_->verify_refresh_token(refresh_token, matching_device, user_info))
            << "A wrong-device refresh attempt revokes the refresh token.";
}

TEST_F(AuthTokenTest, GenerateTokenPairAndRefreshAccessToken) {
    auto tokens = auth_->generate_tokens(user_id_, username_, device_id_, platform_);
    ASSERT_TRUE(tokens.success);
    ASSERT_FALSE(tokens.new_access_token.empty());
    ASSERT_FALSE(tokens.new_refresh_token.empty());

    auto device_id = device_id_;
    auto refreshed = auth_->refresh_access_token(tokens.new_refresh_token, device_id);
    EXPECT_TRUE(refreshed.success);
    EXPECT_FALSE(refreshed.new_access_token.empty());

    im::gateway::UserTokenInfo user_info;
    EXPECT_TRUE(auth_->verify_access_token(refreshed.new_access_token, user_info));
    EXPECT_EQ(user_info.user_id, user_id_);
}

TEST_F(AuthTokenTest, RevokeAndUnrevokeAccessToken) {
    auto token = auth_->generate_access_token(user_id_, username_, device_id_, platform_, 60);
    ASSERT_FALSE(token.empty());

    EXPECT_TRUE(auth_->revoke_token(token));
    EXPECT_TRUE(auth_->is_token_revoked(token));

    im::gateway::UserTokenInfo user_info;
    EXPECT_FALSE(auth_->verify_access_token(token, user_info));

    EXPECT_TRUE(auth_->unrevoke_token(token));
    EXPECT_FALSE(auth_->is_token_revoked(token));
    EXPECT_TRUE(auth_->verify_access_token(token, user_info));
}

TEST_F(AuthTokenTest, DeleteRefreshToken) {
    auto refresh_token =
            auth_->generate_refresh_token(user_id_, username_, device_id_, platform_, 60);
    ASSERT_FALSE(refresh_token.empty());

    EXPECT_TRUE(auth_->del_refresh_token(refresh_token));

    im::gateway::UserTokenInfo user_info;
    auto device_id = device_id_;
    EXPECT_FALSE(auth_->verify_refresh_token(refresh_token, device_id, user_info));
}

TEST_F(AuthTokenTest, RefreshTokenCreatesPerTokenKey) {
    auto rt = auth_->generate_refresh_token(user_id_, username_, device_id_, platform_, 60);
    ASSERT_FALSE(rt.empty());

    bool key_exists = im::db::redis_manager().execute(
            [&](auto& redis) { return redis.exists("refresh_token:" + rt); });
    EXPECT_TRUE(key_exists) << "Per-token key refresh_token:<token> must exist after generation";

    bool old_hash_exists = im::db::redis_manager().execute(
            [&](auto& redis) { return redis.exists("refresh_tokens"); });
    EXPECT_FALSE(old_hash_exists)
            << "Shared refresh_tokens hash must not be used by new tokens";
}

TEST_F(AuthTokenTest, IndependentExpiryPerRefreshToken) {
    auto rt1 = auth_->generate_refresh_token(user_id_, username_, device_id_, platform_, 60);
    ASSERT_FALSE(rt1.empty());

    auto rt2 = auth_->generate_refresh_token(user_id_, username_, device_id_, platform_, 2);
    ASSERT_FALSE(rt2.empty());

    std::this_thread::sleep_for(std::chrono::milliseconds(3100));

    im::gateway::UserTokenInfo user_info;
    auto dev = device_id_;
    EXPECT_TRUE(auth_->verify_refresh_token(rt1, dev, user_info))
            << "rt1 (60s TTL) should still be valid after rt2 (2s TTL) expires";
    EXPECT_FALSE(auth_->verify_refresh_token(rt2, dev, user_info))
            << "rt2 (2s TTL) should be expired";
}

TEST_F(AuthTokenTest, WrongDeviceRevokesOnlyThatToken) {
    auto rt1 = auth_->generate_refresh_token(user_id_, username_, "dev-1", platform_, 60);
    ASSERT_FALSE(rt1.empty());
    auto rt2 = auth_->generate_refresh_token(user_id_, username_, "dev-2", platform_, 60);
    ASSERT_FALSE(rt2.empty());

    im::gateway::UserTokenInfo user_info;
    auto wrong_dev = std::string("wrong-device");
    EXPECT_FALSE(auth_->verify_refresh_token(rt1, wrong_dev, user_info));

    auto dev2 = std::string("dev-2");
    EXPECT_TRUE(auth_->verify_refresh_token(rt2, dev2, user_info))
            << "rt2 should still be valid after wrong-device attempt on rt1";

    auto dev1 = std::string("dev-1");
    EXPECT_FALSE(auth_->verify_refresh_token(rt1, dev1, user_info))
            << "rt1 should be revoked after wrong-device attempt";
}

TEST_F(AuthTokenTest, RevokedAccessTokenCreatesPerJtiKey) {
    auto token = auth_->generate_access_token(user_id_, username_, device_id_, platform_, 60);
    ASSERT_FALSE(token.empty());

    EXPECT_TRUE(auth_->revoke_token(token));
    EXPECT_TRUE(auth_->is_token_revoked(token));

    std::string jti;
    {
        auto decoded = jwt::decode(token);
        jti = decoded.get_id();
    }
    bool jti_exists = im::db::redis_manager().execute(
            [&](auto& redis) { return redis.exists("revoked_access_token:" + jti); });
    EXPECT_TRUE(jti_exists) << "revoked_access_token:<jti> key must exist after revoke";

    EXPECT_TRUE(auth_->unrevoke_token(token));
    EXPECT_FALSE(auth_->is_token_revoked(token));

    bool jti_gone = im::db::redis_manager().execute(
            [&](auto& redis) { return redis.exists("revoked_access_token:" + jti); });
    EXPECT_FALSE(jti_gone) << "revoked_access_token:<jti> key must be deleted after unrevoke";
}

TEST_F(AuthTokenTest, ConcurrentTokenLifecycleUsesRedisPool) {
    auto initial = im::db::redis_manager().get_pool_stats();
    EXPECT_EQ(initial.total_connections, 4u);
    EXPECT_EQ(initial.available_connections, 4u);
    EXPECT_EQ(initial.active_connections, 0u);

    constexpr int kWorkers = 8;
    std::vector<std::future<bool>> futures;
    futures.reserve(kWorkers);

    for (int i = 0; i < kWorkers; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i] {
            const std::string uid = user_id_ + "-concurrent-" + std::to_string(i);
            const std::string username = username_ + std::to_string(i);
            const std::string device = device_id_ + "-" + std::to_string(i);

            auto tokens = auth_->generate_tokens(uid, username, device, platform_);
            if (!tokens.success || tokens.new_access_token.empty() ||
                tokens.new_refresh_token.empty()) {
                return false;
            }

            im::gateway::UserTokenInfo access_info;
            if (!auth_->verify_access_token(tokens.new_access_token, access_info)) {
                return false;
            }
            if (access_info.user_id != uid || access_info.device_id != device) {
                return false;
            }

            im::gateway::UserTokenInfo refresh_info;
            auto refresh_device = device;
            if (!auth_->verify_refresh_token(tokens.new_refresh_token, refresh_device,
                                            refresh_info)) {
                return false;
            }
            if (refresh_info.user_id != uid || refresh_info.device_id != device) {
                return false;
            }

            if (!auth_->revoke_token(tokens.new_access_token)) {
                return false;
            }
            if (!auth_->is_token_revoked(tokens.new_access_token)) {
                return false;
            }
            if (!auth_->unrevoke_token(tokens.new_access_token)) {
                return false;
            }
            if (auth_->is_token_revoked(tokens.new_access_token)) {
                return false;
            }

            return auth_->del_refresh_token(tokens.new_refresh_token);
        }));
    }

    for (auto& future : futures) {
        EXPECT_TRUE(future.get());
    }

    auto final = im::db::redis_manager().get_pool_stats();
    EXPECT_EQ(final.total_connections, 4u);
    EXPECT_EQ(final.available_connections, 4u);
    EXPECT_EQ(final.active_connections, 0u);
}
