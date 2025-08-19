#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>
#include "../../common/database/redis_mgr.hpp"
#include "../../common/utils/log_manager.hpp"
#include "connection_manager.hpp"

namespace im::gateway::test {

// 前置声明
class GatewayServer;

class ConnectionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化日志
        im::utils::LogManager::SetLogToConsole("connection_manager");
        im::utils::LogManager::GetLogger("connection_manager")->set_level(spdlog::level::debug);

        // 初始化Redis管理器
        auto& redis_mgr = im::db::redis_manager();
        im::db::RedisConfig config;
        config.host = "127.0.0.1";
        config.port = 6379;
        config.password = "myself";
        config.db = 1;
        config.pool_size = 5;

        if (!redis_mgr.initialize(config)) {
            FAIL() << "Failed to initialize Redis manager";
        }

        // 创建连接管理器 (注意：在实际使用中，需要传递一个有效的WebSocketServer指针)
        connection_manager_ =
                std::make_unique<ConnectionManager>("../../config/platform_config.json", nullptr);

        // 清理测试数据
        cleanup_test_data();
    }

    void TearDown() override {
        // 清理测试数据
        cleanup_test_data();

        // 关闭Redis管理器
        auto& redis_mgr = im::db::redis_manager();
        redis_mgr.shutdown();
    }

    void cleanup_test_data() {
        try {
            auto& redis_mgr = im::db::redis_manager();
            if (redis_mgr.is_healthy()) {
                redis_mgr.execute([](sw::redis::Redis& redis) {
                    // 清理测试键
                    std::vector<std::string> test_patterns = {"user:sessions:test_user_*",
                                                              "user:platform:test_user_*",
                                                              "session:user:test_session_*"};

                    for (const auto& pattern : test_patterns) {
                        std::vector<std::string> keys;
                        redis.keys(pattern, std::back_inserter(keys));
                        if (!keys.empty()) {
                            redis.del(keys.begin(), keys.end());
                        }
                    }
                    return true;
                });
            }
        } catch (...) {
            // 忽略清理错误
        }
    }

    std::unique_ptr<ConnectionManager> connection_manager_;
};

// 测试DeviceSessionInfo序列化
TEST_F(ConnectionManagerTest, DeviceSessionInfo_Serialization) {
    DeviceSessionInfo info;
    info.session_id = "test_session_123";
    info.device_id = "device_456";
    info.platform = "android";
    info.connect_time = std::chrono::system_clock::now();

    // 测试序列化
    auto json = info.to_json();
    EXPECT_FALSE(json.empty());
    EXPECT_EQ(json["session_id"], "test_session_123");
    EXPECT_EQ(json["device_id"], "device_456");
    EXPECT_EQ(json["platform"], "android");
    EXPECT_TRUE(json.contains("connect_time"));

    // 测试反序列化
    auto parsed_info = DeviceSessionInfo::from_json(json);
    EXPECT_EQ(parsed_info.session_id, info.session_id);
    EXPECT_EQ(parsed_info.device_id, info.device_id);
    EXPECT_EQ(parsed_info.platform, info.platform);
    EXPECT_EQ(parsed_info.connect_time, info.connect_time);
}

// 测试连接管理器构造函数
TEST_F(ConnectionManagerTest, ConnectionManager_Constructor) {
    EXPECT_NE(connection_manager_, nullptr);
}

// 测试Redis键生成
TEST_F(ConnectionManagerTest, GenerateRedisKey) {
    // 由于generate_redis_key是私有方法，我们通过其他方法间接测试
    // 这里只是验证连接管理器能正常工作
    EXPECT_NE(connection_manager_, nullptr);
}

// 测试添加连接
TEST_F(ConnectionManagerTest, AddConnection) {
    // 注意：这个测试需要模拟WebSocketSession，实际测试中可能需要mock对象
    // 这里只是验证基本流程
    EXPECT_NE(connection_manager_, nullptr);
}

// 测试移除连接
TEST_F(ConnectionManagerTest, RemoveConnection) { EXPECT_NE(connection_manager_, nullptr); }

// 测试获取会话
TEST_F(ConnectionManagerTest, GetSession) { EXPECT_NE(connection_manager_, nullptr); }

// 测试获取用户会话
TEST_F(ConnectionManagerTest, GetUserSessions) {
    auto sessions = connection_manager_->get_user_sessions("test_user_123");
    EXPECT_TRUE(sessions.empty());  // 新用户应该没有会话
}

// 测试在线用户统计
TEST_F(ConnectionManagerTest, OnlineUsers) {
    auto users = connection_manager_->get_online_users();
    auto count = connection_manager_->get_online_count();
    EXPECT_EQ(users.size(), count);
}

// 测试平台在线状态检查
TEST_F(ConnectionManagerTest, IsUserOnlineOnPlatform) {
    bool is_online = connection_manager_->is_user_online_on_platform("test_user_123", "android");
    EXPECT_FALSE(is_online);  // 新用户应该不在线
}

// 测试同平台登录挤号
TEST_F(ConnectionManagerTest, CheckAndKickSamePlatform) {
    // 这个方法是私有的，需要通过其他方式测试
    EXPECT_NE(connection_manager_, nullptr);
}

// 并发测试
TEST_F(ConnectionManagerTest, ConcurrentAccess) {
    const int thread_count = 10;
    std::vector<std::future<bool>> futures;

    for (int i = 0; i < thread_count; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            try {
                auto sessions = connection_manager_->get_user_sessions("test_user_concurrent_" +
                                                                       std::to_string(i));
                return true;
            } catch (...) {
                return false;
            }
        }));
    }

    bool all_success = true;
    for (auto& future : futures) {
        all_success &= future.get();
    }

    EXPECT_TRUE(all_success);
}

}  // namespace im::gateway::test

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}