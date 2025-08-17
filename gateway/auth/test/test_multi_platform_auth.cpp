/******************************************************************************
 *
 * @file       test_multi_platform_auth.cpp
 * @brief      多平台认证管理器测试
 *
 * @author     myself
 * @date       2025/08/12
 *
 *****************************************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "multi_platform_auth.hpp"
#include "../../common/database/redis_mgr.hpp"
#include "../../common/utils/log_manager.hpp"

using namespace im::gateway;
using namespace im::db;

class MultiPlatformAuthTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化日志系统
        im::utils::LogManager::SetLogToConsole("auth_mgr");
        
        // 初始化Redis
        bool redis_init = RedisManager::GetInstance().initialize("../../../common/database/config.json");
        ASSERT_TRUE(redis_init) << "Failed to initialize Redis";
        
        // 创建认证管理器
        auth_manager_ = std::make_unique<MultiPlatformAuthManager>(
            "test_secret_key_for_testing_12345678", 
            "platform_token_config.json"
        );
        
        // 清理Redis中的测试数据
        cleanup_redis();
    }
    
    void TearDown() override {
        cleanup_redis();
        RedisManager::GetInstance().shutdown();
    }
    
    void cleanup_redis() {
        try {
            RedisManager::GetInstance().execute([](auto& redis) {
                redis.del("refresh_tokens");
                redis.del("revoked_access_tokens");
                redis.del("user:test_user_id:rt");
            });
        } catch (...) {
            // 忽略清理错误
        }
    }

protected:
    std::unique_ptr<MultiPlatformAuthManager> auth_manager_;
    const std::string test_user_id_ = "test_user_id";
    const std::string test_username_ = "test_user";
    const std::string test_device_id_ = "test_device_123";
    const std::string test_platform_ = "web";
};

// ============ Token 生成测试 ============

TEST_F(MultiPlatformAuthTest, GenerateAccessToken_ValidInput_Success) {
    std::string access_token = auth_manager_->generate_access_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    EXPECT_FALSE(access_token.empty());
    
    // 验证生成的token
    UserTokenInfo user_info;
    bool verify_result = auth_manager_->verify_access_token(access_token, user_info);
    
    EXPECT_TRUE(verify_result);
    EXPECT_EQ(user_info.user_id, test_user_id_);
    EXPECT_EQ(user_info.username, test_username_);
    EXPECT_EQ(user_info.device_id, test_device_id_);
    EXPECT_EQ(user_info.platform, test_platform_);
}

TEST_F(MultiPlatformAuthTest, GenerateRefreshToken_ValidInput_Success) {
    std::string refresh_token = auth_manager_->generate_refresh_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    EXPECT_FALSE(refresh_token.empty());
    
    // 验证refresh token存储在Redis中
    auto meta = RedisManager::GetInstance().execute([&](auto& redis) {
        return redis.hget("refresh_tokens", refresh_token);
    });
    
    // EXPECT_FALSE(meta.empty());
    
    // 验证refresh token
    UserTokenInfo user_info;
    std::string device_id = test_device_id_;
    bool verify_result = auth_manager_->verify_refresh_token(refresh_token, device_id, user_info);
    
    EXPECT_TRUE(verify_result);
    EXPECT_EQ(user_info.user_id, test_user_id_);
    EXPECT_EQ(user_info.username, test_username_);
    EXPECT_EQ(user_info.device_id, test_device_id_);
    EXPECT_EQ(user_info.platform, test_platform_);
}

TEST_F(MultiPlatformAuthTest, GenerateTokens_ValidInput_BothTokensGenerated) {
    TokenResult result = auth_manager_->generate_tokens(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.new_access_token.empty());
    EXPECT_FALSE(result.new_refresh_token.empty());
    EXPECT_TRUE(result.error_message.empty());
}

// ============ Token 验证测试 ============

TEST_F(MultiPlatformAuthTest, VerifyAccessToken_ValidToken_Success) {
    std::string access_token = auth_manager_->generate_access_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    UserTokenInfo user_info;
    bool result = auth_manager_->verify_access_token(access_token, user_info);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(user_info.user_id, test_user_id_);
    EXPECT_EQ(user_info.username, test_username_);
    EXPECT_EQ(user_info.device_id, test_device_id_);
    EXPECT_EQ(user_info.platform, test_platform_);
}

TEST_F(MultiPlatformAuthTest, VerifyAccessToken_InvalidToken_Failure) {
    UserTokenInfo user_info;
    bool result = auth_manager_->verify_access_token("invalid_token", user_info);
    
    EXPECT_FALSE(result);
}

TEST_F(MultiPlatformAuthTest, VerifyRefreshToken_ValidToken_Success) {
    std::string refresh_token = auth_manager_->generate_refresh_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    UserTokenInfo user_info;
    std::string device_id = test_device_id_;
    bool result = auth_manager_->verify_refresh_token(refresh_token, device_id, user_info);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(user_info.user_id, test_user_id_);
    EXPECT_EQ(user_info.username, test_username_);
    EXPECT_EQ(user_info.device_id, test_device_id_);
    EXPECT_EQ(user_info.platform, test_platform_);
}

TEST_F(MultiPlatformAuthTest, VerifyRefreshToken_WrongDeviceId_Failure) {
    std::string refresh_token = auth_manager_->generate_refresh_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    UserTokenInfo user_info;
    std::string wrong_device_id = "wrong_device_id";
    bool result = auth_manager_->verify_refresh_token(refresh_token, wrong_device_id, user_info);
    
    EXPECT_FALSE(result);
}

// ============ Token 刷新测试 ============

TEST_F(MultiPlatformAuthTest, RefreshAccessToken_ValidRefreshToken_Success) {
    // 生成初始tokens
    TokenResult initial_result = auth_manager_->generate_tokens(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    ASSERT_TRUE(initial_result.success);
    
    // 刷新access token
    std::string device_id = test_device_id_;
    TokenResult refresh_result = auth_manager_->refresh_access_token(
        initial_result.new_refresh_token, device_id);
    
    EXPECT_TRUE(refresh_result.success);
    EXPECT_FALSE(refresh_result.new_access_token.empty());
    EXPECT_TRUE(refresh_result.error_message.empty());
    
    // 验证新的access token
    UserTokenInfo user_info;
    bool verify_result = auth_manager_->verify_access_token(
        refresh_result.new_access_token, user_info);
    
    EXPECT_TRUE(verify_result);
    EXPECT_EQ(user_info.user_id, test_user_id_);
}

TEST_F(MultiPlatformAuthTest, RefreshAccessToken_InvalidRefreshToken_Failure) {
    std::string device_id = test_device_id_;
    TokenResult result = auth_manager_->refresh_access_token("invalid_refresh_token", device_id);
    
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.new_access_token.empty());
    EXPECT_FALSE(result.error_message.empty());
}

// ============ Token 撤销测试 ============

TEST_F(MultiPlatformAuthTest, RevokeAccessToken_ValidToken_Success) {
    std::string access_token = auth_manager_->generate_access_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    // 撤销token
    bool revoke_result = auth_manager_->revoke_token(access_token);
    EXPECT_TRUE(revoke_result);
    
    // 验证token已被撤销
    bool is_revoked = auth_manager_->is_token_revoked(access_token);
    EXPECT_TRUE(is_revoked);
    
    // 验证撤销的token无法通过验证
    UserTokenInfo user_info;
    bool verify_result = auth_manager_->verify_access_token(access_token, user_info);
    EXPECT_FALSE(verify_result);
}

TEST_F(MultiPlatformAuthTest, UnrevokeAccessToken_RevokedToken_Success) {
    std::string access_token = auth_manager_->generate_access_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    // 撤销token
    auth_manager_->revoke_token(access_token);
    EXPECT_TRUE(auth_manager_->is_token_revoked(access_token));
    
    // 取消撤销
    bool unrevoke_result = auth_manager_->unrevoke_token(access_token);
    EXPECT_TRUE(unrevoke_result);
    
    // 验证token不再被撤销
    bool is_revoked = auth_manager_->is_token_revoked(access_token);
    EXPECT_FALSE(is_revoked);
    
    // 验证token可以正常使用
    UserTokenInfo user_info;
    bool verify_result = auth_manager_->verify_access_token(access_token, user_info);
    EXPECT_TRUE(verify_result);
}

TEST_F(MultiPlatformAuthTest, RevokeRefreshToken_ValidToken_Success) {
    std::string refresh_token = auth_manager_->generate_refresh_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    // 撤销refresh token
    bool revoke_result = auth_manager_->revoke_refresh_token(refresh_token);
    EXPECT_TRUE(revoke_result);
    
    // 验证refresh token已被撤销
    UserTokenInfo user_info;
    std::string device_id = test_device_id_;
    bool verify_result = auth_manager_->verify_refresh_token(refresh_token, device_id, user_info);
    EXPECT_FALSE(verify_result);
}

TEST_F(MultiPlatformAuthTest, UnrevokeRefreshToken_RevokedToken_Success) {
    std::string refresh_token = auth_manager_->generate_refresh_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    // 撤销refresh token
    auth_manager_->revoke_refresh_token(refresh_token);
    
    // 取消撤销
    bool unrevoke_result = auth_manager_->unrevoke_refresh_token(refresh_token);
    EXPECT_TRUE(unrevoke_result);
    
    // 验证refresh token可以正常使用
    UserTokenInfo user_info;
    std::string device_id = test_device_id_;
    bool verify_result = auth_manager_->verify_refresh_token(refresh_token, device_id, user_info);
    EXPECT_TRUE(verify_result);
}

TEST_F(MultiPlatformAuthTest, DeleteRefreshToken_ValidToken_Success) {
    std::string refresh_token = auth_manager_->generate_refresh_token(
        test_user_id_, test_username_, test_device_id_, test_platform_);
    
    // 删除refresh token
    bool delete_result = auth_manager_->del_refresh_token(refresh_token);
    EXPECT_TRUE(delete_result);
    
    // 验证refresh token已被删除
    UserTokenInfo user_info;
    std::string device_id = test_device_id_;
    bool verify_result = auth_manager_->verify_refresh_token(refresh_token, device_id, user_info);
    EXPECT_FALSE(verify_result);
}

// ============ 平台配置测试 ============

TEST_F(MultiPlatformAuthTest, DifferentPlatforms_DifferentConfigurations) {
    std::vector<std::string> platforms = {"web", "android", "ios", "desktop", "miniapp"};
    
    for (const auto& platform : platforms) {
        TokenResult result = auth_manager_->generate_tokens(
            test_user_id_, test_username_, test_device_id_, platform);
        
        EXPECT_TRUE(result.success) << "Failed for platform: " << platform;
        EXPECT_FALSE(result.new_access_token.empty()) << "Empty access token for platform: " << platform;
        EXPECT_FALSE(result.new_refresh_token.empty()) << "Empty refresh token for platform: " << platform;
        
        // 验证tokens
        UserTokenInfo user_info;
        bool access_verify = auth_manager_->verify_access_token(result.new_access_token, user_info);
        EXPECT_TRUE(access_verify) << "Access token verification failed for platform: " << platform;
        EXPECT_EQ(user_info.platform, platform);
        
        std::string device_id = test_device_id_;
        bool refresh_verify = auth_manager_->verify_refresh_token(result.new_refresh_token, device_id, user_info);
        EXPECT_TRUE(refresh_verify) << "Refresh token verification failed for platform: " << platform;
        EXPECT_EQ(user_info.platform, platform);
    }
}

// ============ 边界条件测试 ============

TEST_F(MultiPlatformAuthTest, EmptyInputs_HandleGracefully) {
    // 测试空用户ID
    std::string access_token = auth_manager_->generate_access_token("", test_username_, test_device_id_, test_platform_);
    EXPECT_FALSE(access_token.empty()); // JWT库应该能处理空字符串
    
    // 测试空平台
    TokenResult result = auth_manager_->generate_tokens(test_user_id_, test_username_, test_device_id_, "");
    EXPECT_TRUE(result.success); // 应该使用默认配置
}

TEST_F(MultiPlatformAuthTest, LongInputs_HandleCorrectly) {
    std::string long_user_id(1000, 'a');
    std::string long_username(1000, 'b');
    std::string long_device_id(1000, 'c');
    
    TokenResult result = auth_manager_->generate_tokens(long_user_id, long_username, long_device_id, test_platform_);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.new_access_token.empty());
    EXPECT_FALSE(result.new_refresh_token.empty());
    
    // 验证长输入的token
    UserTokenInfo user_info;
    bool verify_result = auth_manager_->verify_access_token(result.new_access_token, user_info);
    EXPECT_TRUE(verify_result);
    EXPECT_EQ(user_info.user_id, long_user_id);
    EXPECT_EQ(user_info.username, long_username);
    EXPECT_EQ(user_info.device_id, long_device_id);
}

// ============ 并发测试 ============

TEST_F(MultiPlatformAuthTest, ConcurrentTokenGeneration_ThreadSafe) {
    const int num_threads = 10;
    const int tokens_per_thread = 10;
    std::vector<std::thread> threads;
    std::vector<std::vector<TokenResult>> results(num_threads);
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &results, i, tokens_per_thread]() {
            for (int j = 0; j < tokens_per_thread; ++j) {
                std::string user_id = test_user_id_ + "_" + std::to_string(i) + "_" + std::to_string(j);
                TokenResult result = auth_manager_->generate_tokens(
                    user_id, test_username_, test_device_id_, test_platform_);
                results[i].push_back(result);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有结果
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < tokens_per_thread; ++j) {
            EXPECT_TRUE(results[i][j].success) 
                << "Failed for thread " << i << ", token " << j;
            EXPECT_FALSE(results[i][j].new_access_token.empty())
                << "Empty access token for thread " << i << ", token " << j;
            EXPECT_FALSE(results[i][j].new_refresh_token.empty())
                << "Empty refresh token for thread " << i << ", token " << j;
        }
    }
}

// ============ 性能测试 ============

TEST_F(MultiPlatformAuthTest, DISABLED_PerformanceTest_TokenGeneration) {
    const int num_iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_iterations; ++i) {
        std::string user_id = test_user_id_ + "_" + std::to_string(i);
        TokenResult result = auth_manager_->generate_tokens(
            user_id, test_username_, test_device_id_, test_platform_);
        ASSERT_TRUE(result.success);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Generated " << num_iterations << " token pairs in " 
              << duration.count() << "ms" << std::endl;
    std::cout << "Average: " << (double)duration.count() / num_iterations 
              << "ms per token pair" << std::endl;
    
    // 性能要求：平均每个token生成时间应该小于50ms
    EXPECT_LT((double)duration.count() / num_iterations, 50.0);
}

TEST_F(MultiPlatformAuthTest, DISABLED_PerformanceTest_TokenVerification) {
    // 预生成tokens
    const int num_tokens = 1000;
    std::vector<std::string> access_tokens;
    
    for (int i = 0; i < num_tokens; ++i) {
        std::string user_id = test_user_id_ + "_" + std::to_string(i);
        std::string access_token = auth_manager_->generate_access_token(
            user_id, test_username_, test_device_id_, test_platform_);
        access_tokens.push_back(access_token);
    }
    
    // 测试验证性能
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& token : access_tokens) {
        UserTokenInfo user_info;
        bool result = auth_manager_->verify_access_token(token, user_info);
        ASSERT_TRUE(result);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Verified " << num_tokens << " tokens in " 
              << duration.count() << "ms" << std::endl;
    std::cout << "Average: " << (double)duration.count() / num_tokens 
              << "ms per verification" << std::endl;
    
    // 性能要求：平均每个token验证时间应该小于10ms
    EXPECT_LT((double)duration.count() / num_tokens, 10.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}