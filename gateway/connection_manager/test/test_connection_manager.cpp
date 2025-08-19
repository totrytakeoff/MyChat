#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "mock_objects.hpp"
#include "../connection_manager.hpp"

using namespace im::gateway;
using namespace im::gateway::test;
using namespace testing;

class ConnectionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建mock对象
        mock_websocket_server_ = std::make_unique<MockWebSocketServer>();
        mock_platform_strategy_ = std::make_unique<MockPlatformTokenStrategy>();
        
        // 设置默认行为
        mock_platform_strategy_->SetupDefaultConfig();
        
        // 创建平台配置文件
        CreateMockPlatformConfig();
        
        // 初始化ConnectionManager
        connection_manager_ = std::make_unique<ConnectionManager>(
            config_path_, 
            reinterpret_cast<im::network::WebSocketServer*>(mock_websocket_server_.get())
        );
    }

    void TearDown() override {
        // 清理测试数据
        connection_manager_.reset();
        mock_websocket_server_.reset();
        mock_platform_strategy_.reset();
    }

    void CreateMockPlatformConfig() {
        config_path_ = "/tmp/test_platform_config.json";
        nlohmann::json config = {
            {"platforms", {
                {"android", {
                    {"enable_multi_device", true},
                    {"access_token_expire_time", 3600},
                    {"refresh_token_expire_time", 86400},
                    {"algorithm", "HS256"}
                }},
                {"ios", {
                    {"enable_multi_device", false},
                    {"access_token_expire_time", 3600},
                    {"refresh_token_expire_time", 86400},
                    {"algorithm", "HS256"}
                }},
                {"web", {
                    {"enable_multi_device", true},
                    {"access_token_expire_time", 1800},
                    {"refresh_token_expire_time", 43200},
                    {"algorithm", "HS256"}
                }}
            }}
        };
        
        std::ofstream file(config_path_);
        file << config.dump(4);
        file.close();
    }

    std::shared_ptr<MockWebSocketSession> CreateMockSession(const std::string& session_id = "") {
        auto mock_session = std::make_shared<MockWebSocketSession>();
        if (!session_id.empty()) {
            // 如果需要特定session_id，可以在这里设置
        }
        return mock_session;
    }

protected:
    std::unique_ptr<ConnectionManager> connection_manager_;
    std::unique_ptr<MockWebSocketServer> mock_websocket_server_;
    std::unique_ptr<MockPlatformTokenStrategy> mock_platform_strategy_;
    std::string config_path_;
};

// Test DeviceSessionInfo serialization
TEST_F(ConnectionManagerTest, DeviceSessionInfoSerialization) {
    DeviceSessionInfo info;
    info.session_id = "test_session_123";
    info.device_id = "device_456";
    info.platform = "android";
    info.connect_time = std::chrono::system_clock::now();

    // Test to_json
    auto json_obj = info.to_json();
    EXPECT_EQ(json_obj["session_id"], "test_session_123");
    EXPECT_EQ(json_obj["device_id"], "device_456");
    EXPECT_EQ(json_obj["platform"], "android");
    EXPECT_TRUE(json_obj.contains("connect_time"));

    // Test from_json
    auto restored_info = DeviceSessionInfo::from_json(json_obj);
    EXPECT_EQ(restored_info.session_id, info.session_id);
    EXPECT_EQ(restored_info.device_id, info.device_id);
    EXPECT_EQ(restored_info.platform, info.platform);
    
    // 时间精度可能有毫秒级差异，这里检查时间差在合理范围内
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        info.connect_time - restored_info.connect_time).count();
    EXPECT_LE(std::abs(time_diff), 1000); // 1秒内的差异是可接受的
}

// Test add_connection with multi-device support
TEST_F(ConnectionManagerTest, AddConnectionMultiDevice) {
    auto session1 = CreateMockSession();
    auto session2 = CreateMockSession();
    
    // Mock Redis operations
    MockRedisManager& redis = MockRedisManager::GetInstance();
    EXPECT_CALL(redis, hset(_, _, _)).Times(AtLeast(2));
    EXPECT_CALL(redis, sadd(_, _)).Times(AtLeast(2));
    
    // Mock WebSocket server
    mock_websocket_server_->add_session_internal(session1->get_session_id(), session1);
    mock_websocket_server_->add_session_internal(session2->get_session_id(), session2);
    
    // 添加第一个连接
    bool result1 = connection_manager_->add_connection(
        "user123", "device1", "android", session1);
    EXPECT_TRUE(result1);
    
    // 添加同一用户的另一个设备连接（Android支持多设备）
    bool result2 = connection_manager_->add_connection(
        "user123", "device2", "android", session2);
    EXPECT_TRUE(result2);
}

// Test add_connection with single device platform (kick old session)
TEST_F(ConnectionManagerTest, AddConnectionSingleDeviceKickOld) {
    // 设置iOS为单设备平台
    mock_platform_strategy_->SetupSingleDeviceConfig();
    
    auto old_session = CreateMockSession();
    auto new_session = CreateMockSession();
    
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    // Mock finding existing session
    std::unordered_map<std::string, std::string> existing_sessions = {
        {"device1:ios", R"({"session_id":"old_session","device_id":"device1","platform":"ios","connect_time":1234567890000})"}
    };
    
    EXPECT_CALL(redis, hgetall(_))
        .WillOnce(Return(existing_sessions));
    
    // Mock Redis operations for adding new connection
    EXPECT_CALL(redis, hset(_, _, _)).Times(AtLeast(1));
    EXPECT_CALL(redis, sadd(_, _)).Times(AtLeast(1));
    
    // Mock WebSocket server operations
    mock_websocket_server_->add_session_internal("old_session", old_session);
    mock_websocket_server_->add_session_internal(new_session->get_session_id(), new_session);
    
    EXPECT_CALL(*old_session, close()).Times(1);
    
    // 添加新连接，应该踢掉旧连接
    bool result = connection_manager_->add_connection(
        "user123", "device2", "ios", new_session);
    EXPECT_TRUE(result);
}

// Test remove_connection by user_id and device_id
TEST_F(ConnectionManagerTest, RemoveConnectionByUserAndDevice) {
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    // Mock existing sessions
    std::unordered_map<std::string, std::string> sessions = {
        {"device1:android", R"({"session_id":"session123","device_id":"device1","platform":"android","connect_time":1234567890000})"},
        {"device2:android", R"({"session_id":"session456","device_id":"device2","platform":"android","connect_time":1234567890001})"}
    };
    
    EXPECT_CALL(redis, hgetall(_))
        .WillOnce(Return(sessions));
    
    EXPECT_CALL(redis, hdel(_, "device1:android")).Times(1);
    EXPECT_CALL(redis, srem(_, "device1:android")).Times(1);
    EXPECT_CALL(redis, del(_)).Times(1); // session:user key deletion
    
    connection_manager_->remove_connection("user123", "device1");
}

// Test remove_connection by session
TEST_F(ConnectionManagerTest, RemoveConnectionBySession) {
    auto session = CreateMockSession();
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    // Mock session to user mapping
    std::unordered_map<std::string, std::string> session_info = {
        {"user_id", "user123"},
        {"device_id", "device1"},
        {"platform", "android"}
    };
    
    EXPECT_CALL(redis, hgetall(_))
        .WillRepeatedly(Return(session_info));
    
    EXPECT_CALL(redis, hdel(_, _)).Times(AtLeast(1));
    EXPECT_CALL(redis, srem(_, _)).Times(AtLeast(1));
    EXPECT_CALL(redis, del(_)).Times(AtLeast(1));
    
    connection_manager_->remove_connection(session);
}

// Test get_session
TEST_F(ConnectionManagerTest, GetSession) {
    auto session = CreateMockSession();
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    // Mock Redis response
    std::string session_json = R"({"session_id":")" + session->get_session_id() + 
                              R"(","device_id":"device1","platform":"android","connect_time":1234567890000})";
    
    EXPECT_CALL(redis, hget(_, _))
        .WillOnce(Return(std::optional<std::string>(session_json)));
    
    // Mock WebSocket server
    mock_websocket_server_->add_session_internal(session->get_session_id(), session);
    
    auto result = connection_manager_->get_session("user123", "device1", "android");
    EXPECT_EQ(result, session);
}

// Test get_session not found
TEST_F(ConnectionManagerTest, GetSessionNotFound) {
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    EXPECT_CALL(redis, hget(_, _))
        .WillOnce(Return(std::nullopt));
    
    auto result = connection_manager_->get_session("user123", "device1", "android");
    EXPECT_EQ(result, nullptr);
}

// Test get_user_sessions
TEST_F(ConnectionManagerTest, GetUserSessions) {
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    // Mock multiple sessions
    std::unordered_map<std::string, std::string> sessions = {
        {"device1:android", R"({"session_id":"session1","device_id":"device1","platform":"android","connect_time":1234567890000})"},
        {"device2:ios", R"({"session_id":"session2","device_id":"device2","platform":"ios","connect_time":1234567890001})"}
    };
    
    EXPECT_CALL(redis, hgetall(_))
        .WillOnce(Return(sessions));
    
    auto result = connection_manager_->get_user_sessions("user123");
    EXPECT_EQ(result.size(), 2);
    
    // 验证返回的会话信息
    bool found_android = false, found_ios = false;
    for (const auto& session_info : result) {
        if (session_info.platform == "android") {
            EXPECT_EQ(session_info.session_id, "session1");
            EXPECT_EQ(session_info.device_id, "device1");
            found_android = true;
        } else if (session_info.platform == "ios") {
            EXPECT_EQ(session_info.session_id, "session2");
            EXPECT_EQ(session_info.device_id, "device2");
            found_ios = true;
        }
    }
    EXPECT_TRUE(found_android);
    EXPECT_TRUE(found_ios);
}

// Test get_online_users
TEST_F(ConnectionManagerTest, GetOnlineUsers) {
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    std::unordered_set<std::string> online_users = {"user1", "user2", "user3"};
    
    EXPECT_CALL(redis, smembers("online:users"))
        .WillOnce(Return(online_users));
    
    auto result = connection_manager_->get_online_users();
    EXPECT_EQ(result.size(), 3);
    
    std::sort(result.begin(), result.end());
    std::vector<std::string> expected = {"user1", "user2", "user3"};
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(result, expected);
}

// Test get_online_count
TEST_F(ConnectionManagerTest, GetOnlineCount) {
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    EXPECT_CALL(redis, scard("online:users"))
        .WillOnce(Return(42));
    
    auto count = connection_manager_->get_online_count();
    EXPECT_EQ(count, 42);
}

// Test is_user_online_on_platform
TEST_F(ConnectionManagerTest, IsUserOnlineOnPlatform) {
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    std::unordered_set<std::string> devices = {"device1:android", "device2:ios"};
    
    EXPECT_CALL(redis, smembers(_))
        .WillOnce(Return(devices));
    
    // 测试用户在Android平台在线
    bool result1 = connection_manager_->is_user_online_on_platform("user123", "android");
    EXPECT_TRUE(result1);
    
    // 再次调用测试iOS平台
    EXPECT_CALL(redis, smembers(_))
        .WillOnce(Return(devices));
    
    bool result2 = connection_manager_->is_user_online_on_platform("user123", "ios");
    EXPECT_TRUE(result2);
    
    // 测试用户不在web平台在线
    EXPECT_CALL(redis, smembers(_))
        .WillOnce(Return(devices));
    
    bool result3 = connection_manager_->is_user_online_on_platform("user123", "web");
    EXPECT_FALSE(result3);
}

// Test Redis error handling
TEST_F(ConnectionManagerTest, RedisErrorHandling) {
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    // Mock Redis throwing exception
    EXPECT_CALL(redis, hset(_, _, _))
        .WillOnce(Throw(std::runtime_error("Redis connection failed")));
    
    auto session = CreateMockSession();
    
    // 应该返回false当Redis操作失败时
    bool result = connection_manager_->add_connection(
        "user123", "device1", "android", session);
    EXPECT_FALSE(result);
}

// Test platform configuration edge cases
TEST_F(ConnectionManagerTest, PlatformConfigurationEdgeCases) {
    // 测试无效平台配置
    auto session = CreateMockSession();
    
    // Mock platform strategy返回默认配置
    PlatformTokenConfig unknown_config;
    unknown_config.platform = PlatformType::UNKNOWN;
    unknown_config.enable_multi_device = false;
    unknown_config.token_time_config.access_token_expire_seconds = 3600;
    unknown_config.token_time_config.refresh_token_expire_seconds = 86400;
    unknown_config.refresh_config.auto_refresh_enabled = true;
    unknown_config.refresh_config.refresh_precentage = 0.2f;
    unknown_config.refresh_config.background_refresh = true;
    unknown_config.refresh_config.max_retry_count = 3;
    
    EXPECT_CALL(*mock_platform_strategy_, get_platform_token_config("unknown_platform"))
        .WillOnce(Return(unknown_config));
    
    MockRedisManager& redis = MockRedisManager::GetInstance();
    EXPECT_CALL(redis, hgetall(_))
        .WillOnce(Return(std::unordered_map<std::string, std::string>{}));
    EXPECT_CALL(redis, hset(_, _, _)).Times(AtLeast(1));
    EXPECT_CALL(redis, sadd(_, _)).Times(AtLeast(1));
    
    bool result = connection_manager_->add_connection(
        "user123", "device1", "unknown_platform", session);
    EXPECT_TRUE(result);
}

// Integration test: Complete user session lifecycle
TEST_F(ConnectionManagerTest, CompleteSessionLifecycle) {
    auto session1 = CreateMockSession();
    auto session2 = CreateMockSession();
    MockRedisManager& redis = MockRedisManager::GetInstance();
    
    // Step 1: Add first connection
    EXPECT_CALL(redis, hgetall(_))
        .WillOnce(Return(std::unordered_map<std::string, std::string>{}));
    EXPECT_CALL(redis, hset(_, _, _)).Times(AtLeast(3));
    EXPECT_CALL(redis, sadd(_, _)).Times(AtLeast(1));
    
    mock_websocket_server_->add_session_internal(session1->get_session_id(), session1);
    
    bool add_result1 = connection_manager_->add_connection(
        "user123", "device1", "android", session1);
    EXPECT_TRUE(add_result1);
    
    // Step 2: Add second connection (same user, different device)
    EXPECT_CALL(redis, hgetall(_))
        .WillOnce(Return(std::unordered_map<std::string, std::string>{
            {"device1:android", R"({"session_id":")" + session1->get_session_id() + R"(","device_id":"device1","platform":"android","connect_time":1234567890000})"}
        }));
    EXPECT_CALL(redis, hset(_, _, _)).Times(AtLeast(3));
    EXPECT_CALL(redis, sadd(_, _)).Times(AtLeast(1));
    
    mock_websocket_server_->add_session_internal(session2->get_session_id(), session2);
    
    bool add_result2 = connection_manager_->add_connection(
        "user123", "device2", "android", session2);
    EXPECT_TRUE(add_result2);
    
    // Step 3: Get user sessions
    std::unordered_map<std::string, std::string> all_sessions = {
        {"device1:android", R"({"session_id":")" + session1->get_session_id() + R"(","device_id":"device1","platform":"android","connect_time":1234567890000})"},
        {"device2:android", R"({"session_id":")" + session2->get_session_id() + R"(","device_id":"device2","platform":"android","connect_time":1234567890001})"}
    };
    
    EXPECT_CALL(redis, hgetall(_))
        .WillOnce(Return(all_sessions));
    
    auto sessions = connection_manager_->get_user_sessions("user123");
    EXPECT_EQ(sessions.size(), 2);
    
    // Step 4: Remove one connection
    EXPECT_CALL(redis, hgetall(_))
        .WillOnce(Return(all_sessions));
    EXPECT_CALL(redis, hdel(_, "device1:android")).Times(1);
    EXPECT_CALL(redis, srem(_, "device1:android")).Times(1);
    EXPECT_CALL(redis, del(_)).Times(1);
    
    connection_manager_->remove_connection("user123", "device1");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}