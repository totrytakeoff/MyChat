#include "../../common/database/redis/redis_mgr.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../../gateway/auth/multi_platform_auth.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace {

im::db::RedisConfig test_redis_config() {
    im::db::RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "mychat-dev-pass";
    config.db = 15;
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

    void cleanup() {
        im::db::redis_manager().safe_execute(
                [](auto& redis) {
                    redis.del("refresh_tokens");
                    redis.del("revoked_access_tokens");
                    redis.del("user:test-user:rt");
                    redis.del("user:test-user-2:rt");
                    return true;
                },
                false);
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
