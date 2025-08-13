#include <gtest/gtest.h>
#include <iostream>
#include "redis_mgr.hpp"
#include "log_manager.hpp"

class SimpleRedisTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置日志
        im::utils::LogManager::SetLogToConsole("redis_manager");
        im::utils::LogManager::SetLogToConsole("connection_pool");
    }
};

TEST_F(SimpleRedisTest, BasicConnection) {
    try {
        std::cout << "Testing basic Redis connection..." << std::endl;
        
        // 创建配置
        im::db::RedisConfig config;
        config.host = "127.0.0.1";
        config.port = 6379;
        config.password = "myself";
        config.db = 1;
        config.pool_size = 2;  // 使用小的连接池
        config.connect_timeout = 2000;
        config.socket_timeout = 2000;
        
        // 获取 Redis 管理器
        auto& mgr = im::db::redis_manager();
        
        std::cout << "Initializing Redis manager..." << std::endl;
        bool init_result = mgr.initialize(config);
        
        std::cout << "Initialization result: " << init_result << std::endl;
        EXPECT_TRUE(init_result);
        
        if (init_result) {
            std::cout << "Testing health check..." << std::endl;
            bool health = mgr.is_healthy();
            std::cout << "Health check result: " << health << std::endl;
            EXPECT_TRUE(health);
        }
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        FAIL() << "Exception thrown: " << e.what();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}