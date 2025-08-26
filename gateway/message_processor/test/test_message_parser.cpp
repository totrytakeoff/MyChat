#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <chrono>
#include <thread>

#include "../message_parser.hpp"
#include "mock_http_client.hpp"
#include "mock_websocket_client.hpp"
#include "../../../common/utils/log_manager.hpp"

using namespace im::gateway;
using namespace im::test;
using namespace im::utils;

class MessageParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化日志管理器
        LogManager::SetLogToConsole("message_parser");
        LogManager::SetLogToConsole("protobuf_codec");
        LogManager::SetLogToConsole("router_manager");
        LogManager::SetLoggingEnabled("message_parser", true);
        LogManager::SetLoggingEnabled("protobuf_codec", true);
        LogManager::SetLoggingEnabled("router_manager", true);
        
        // 创建消息解析器实例
        try {
            parser_ = std::make_unique<MessageParser>("test_router_config.json");
        } catch (const std::exception& e) {
            FAIL() << "Failed to initialize MessageParser: " << e.what();
        }
        
        // 创建mock客户端
        http_client_ = std::make_unique<MockHttpClient>();
        websocket_client_ = std::make_unique<MockWebSocketClient>();
    }

    void TearDown() override {
        parser_.reset();
        http_client_.reset();
        websocket_client_.reset();
    }

    std::unique_ptr<MessageParser> parser_;
    std::unique_ptr<MockHttpClient> http_client_;
    std::unique_ptr<MockWebSocketClient> websocket_client_;
};

// ===== HTTP请求解析测试 =====

TEST_F(MessageParserTest, ParseHttpPostLoginRequest) {
    // 创建登录请求
    auto req = http_client_->create_login_request("testuser", "testpass");
    
    // 解析请求
    auto result = parser_->parse_http_request(req);
    
    // 验证结果
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_cmd_id(), 10001); // 登录命令ID
    EXPECT_TRUE(result->is_http());
    EXPECT_FALSE(result->is_websocket());
    EXPECT_EQ(result->get_session_context().http_method, "POST");
    EXPECT_EQ(result->get_session_context().original_path, "/api/v1/user/login");
    EXPECT_TRUE(result->has_json_body());
    EXPECT_FALSE(result->has_protobuf_message());
    
    // 验证JSON内容
    auto json_body = result->get_json_body();
    EXPECT_FALSE(json_body.empty());
    
    // 验证from_uid和to_uid是否正确提取
    EXPECT_EQ(result->get_from_uid(), "testuser");
    EXPECT_EQ(result->get_to_uid(), "system");
}

TEST_F(MessageParserTest, ParseHttpPostRegisterRequest) {
    // 创建注册请求
    auto req = http_client_->create_register_request("newuser", "newpass", "test@example.com");
    
    // 解析请求
    auto result = parser_->parse_http_request(req);
    
    // 验证结果
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_cmd_id(), 10002); // 注册命令ID
    EXPECT_TRUE(result->is_http());
    EXPECT_EQ(result->get_session_context().http_method, "POST");
    EXPECT_EQ(result->get_session_context().original_path, "/api/v1/user/register");
    EXPECT_TRUE(result->has_json_body());
    
    // 验证from_uid和to_uid
    EXPECT_EQ(result->get_from_uid(), "newuser");
    EXPECT_EQ(result->get_to_uid(), "system");
}

TEST_F(MessageParserTest, ParseHttpGetProfileRequest) {
    // 创建获取用户信息请求
    auto req = http_client_->create_get_profile_request("user123", "valid_token");
    
    // 解析请求
    auto result = parser_->parse_http_request(req);
    
    // 验证结果
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_cmd_id(), 10003); // 获取用户信息命令ID
    EXPECT_TRUE(result->is_http());
    EXPECT_EQ(result->get_session_context().http_method, "GET");
    EXPECT_EQ(result->get_session_context().original_path, "/api/v1/user/profile");
    EXPECT_TRUE(result->has_json_body()); // GET参数转换为JSON
    
    // 验证token提取
    EXPECT_EQ(result->get_token(), "valid_token");
    EXPECT_EQ(result->get_device_id(), "profile_device_003");
    EXPECT_EQ(result->get_platform(), "web");
    
    // 验证from_uid和to_uid
    EXPECT_EQ(result->get_from_uid(), "user123");
    EXPECT_EQ(result->get_to_uid(), "system");
}

TEST_F(MessageParserTest, ParseHttpSendMessageRequest) {
    // 创建发送消息请求
    auto req = http_client_->create_send_message_request("sender", "receiver", "Hello!", "msg_token");
    
    // 解析请求
    auto result = parser_->parse_http_request(req);
    
    // 验证结果
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_cmd_id(), 20001); // 发送消息命令ID
    EXPECT_TRUE(result->is_http());
    EXPECT_EQ(result->get_session_context().http_method, "POST");
    EXPECT_EQ(result->get_session_context().original_path, "/api/v1/message/send");
    
    // 验证from_uid和to_uid
    EXPECT_EQ(result->get_from_uid(), "sender");
    EXPECT_EQ(result->get_to_uid(), "receiver");
    EXPECT_EQ(result->get_token(), "msg_token");
}

TEST_F(MessageParserTest, ParseHttpGetMessageHistoryRequest) {
    // 创建获取消息历史请求
    auto req = http_client_->create_get_message_history_request("user1", "user2", 1, 20, "history_token");
    
    // 解析请求
    auto result = parser_->parse_http_request(req);
    
    // 验证结果
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_cmd_id(), 20002); // 获取消息历史命令ID
    EXPECT_TRUE(result->is_http());
    EXPECT_EQ(result->get_session_context().http_method, "GET");
    EXPECT_EQ(result->get_session_context().original_path, "/api/v1/message/history");
    
    // 验证from_uid和to_uid
    EXPECT_EQ(result->get_from_uid(), "user1");
    EXPECT_EQ(result->get_to_uid(), "user2");
    EXPECT_EQ(result->get_token(), "history_token");
}

// ===== HTTP请求解析增强版本测试 =====

TEST_F(MessageParserTest, ParseHttpRequestEnhanced_Success) {
    auto req = http_client_->create_login_request("testuser", "testpass");
    
    auto result = parser_->parse_http_request_enhanced(req);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_code, ParseResult::SUCCESS);
    EXPECT_TRUE(result.error_message.empty());
    ASSERT_NE(result.message, nullptr);
    EXPECT_EQ(result.message->get_cmd_id(), 10001);
}

TEST_F(MessageParserTest, ParseHttpRequestEnhanced_InvalidPath) {
    auto req = http_client_->create_invalid_request();
    
    auto result = parser_->parse_http_request_enhanced(req);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, ParseResult::ROUTING_FAILED);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.message, nullptr);
}

TEST_F(MessageParserTest, ParseHttpRequestEnhanced_EmptyPath) {
    auto req = http_client_->create_empty_path_request();
    
    auto result = parser_->parse_http_request_enhanced(req);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, ParseResult::INVALID_REQUEST);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.message, nullptr);
}

// ===== WebSocket消息解析测试 =====

TEST_F(MessageParserTest, ParseWebSocketMessage_Heartbeat) {
    // 创建心跳消息
    auto ws_message = websocket_client_->create_heartbeat_message();
    ASSERT_FALSE(ws_message.empty());
    
    // 解析消息
    auto result = parser_->parse_websocket_message(ws_message, "ws_session_001");
    
    // 验证结果
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_cmd_id(), 1001); // 心跳命令ID
    EXPECT_TRUE(result->is_websocket());
    EXPECT_FALSE(result->is_http());
    EXPECT_EQ(result->get_session_id(), "ws_session_001");
    EXPECT_TRUE(result->has_raw_protobuf_data());
    EXPECT_EQ(result->get_raw_protobuf_data(), ws_message);
    
    // 验证token和设备信息
    EXPECT_EQ(result->get_token(), "heartbeat_token");
    EXPECT_EQ(result->get_device_id(), "heartbeat_device");
    EXPECT_EQ(result->get_platform(), "test_platform");
}

TEST_F(MessageParserTest, ParseWebSocketMessage_Login) {
    // 创建登录消息
    auto ws_message = websocket_client_->create_login_message("wsuser", "wspass", "ws_login_token");
    ASSERT_FALSE(ws_message.empty());
    
    // 解析消息
    auto result = parser_->parse_websocket_message(ws_message);
    
    // 验证结果
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_cmd_id(), 10001); // 登录命令ID
    EXPECT_TRUE(result->is_websocket());
    EXPECT_EQ(result->get_token(), "ws_login_token");
    EXPECT_EQ(result->get_device_id(), "login_device_001");
    EXPECT_EQ(result->get_platform(), "mobile");
    EXPECT_EQ(result->get_from_uid(), "wsuser");
    EXPECT_TRUE(result->has_raw_protobuf_data());
}

TEST_F(MessageParserTest, ParseWebSocketMessage_Chat) {
    // 创建聊天消息
    auto ws_message = websocket_client_->create_chat_message("ws_sender", "ws_receiver", "Hello WebSocket!", "ws_chat_token");
    ASSERT_FALSE(ws_message.empty());
    
    // 解析消息
    auto result = parser_->parse_websocket_message(ws_message);
    
    // 验证结果
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_cmd_id(), 20001); // 发送消息命令ID
    EXPECT_TRUE(result->is_websocket());
    EXPECT_EQ(result->get_token(), "ws_chat_token");
    EXPECT_EQ(result->get_from_uid(), "ws_sender");
    EXPECT_EQ(result->get_to_uid(), "ws_receiver");
    EXPECT_EQ(result->get_platform(), "web");
    EXPECT_TRUE(result->has_raw_protobuf_data());
}

// ===== WebSocket消息解析增强版本测试 =====

TEST_F(MessageParserTest, ParseWebSocketMessageEnhanced_Success) {
    auto ws_message = websocket_client_->create_heartbeat_message();
    
    auto result = parser_->parse_websocket_message_enhanced(ws_message, "ws_session_enhanced");
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.error_code, ParseResult::SUCCESS);
    EXPECT_TRUE(result.error_message.empty());
    ASSERT_NE(result.message, nullptr);
    EXPECT_EQ(result.message->get_session_id(), "ws_session_enhanced");
}

TEST_F(MessageParserTest, ParseWebSocketMessageEnhanced_EmptyMessage) {
    auto ws_message = websocket_client_->create_empty_message();
    
    auto result = parser_->parse_websocket_message_enhanced(ws_message);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, ParseResult::INVALID_REQUEST);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.message, nullptr);
}

TEST_F(MessageParserTest, ParseWebSocketMessageEnhanced_InvalidMessage) {
    auto ws_message = websocket_client_->create_invalid_message();
    
    auto result = parser_->parse_websocket_message_enhanced(ws_message);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, ParseResult::DECODE_FAILED);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.message, nullptr);
}

TEST_F(MessageParserTest, ParseWebSocketMessageEnhanced_LargeMessage) {
    auto ws_message = websocket_client_->create_large_message();
    
    auto result = parser_->parse_websocket_message_enhanced(ws_message);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, ParseResult::INVALID_REQUEST);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.message, nullptr);
}

// ===== 会话ID生成测试 =====

TEST_F(MessageParserTest, SessionIdGeneration_HTTP) {
    auto req1 = http_client_->create_login_request("user1", "pass1");
    auto req2 = http_client_->create_login_request("user2", "pass2");
    
    auto result1 = parser_->parse_http_request(req1);
    auto result2 = parser_->parse_http_request(req2);
    
    ASSERT_NE(result1, nullptr);
    ASSERT_NE(result2, nullptr);
    
    // 不同请求应该生成不同的会话ID
    EXPECT_NE(result1->get_session_id(), result2->get_session_id());
    
    // HTTP会话ID应该有特定前缀
    EXPECT_THAT(result1->get_session_id(), ::testing::StartsWith("http_"));
    EXPECT_THAT(result2->get_session_id(), ::testing::StartsWith("http_"));
}

TEST_F(MessageParserTest, SessionIdGeneration_WebSocket) {
    auto ws_msg1 = websocket_client_->create_heartbeat_message();
    auto ws_msg2 = websocket_client_->create_login_message("wsuser", "wspass");
    
    auto result1 = parser_->parse_websocket_message(ws_msg1);
    auto result2 = parser_->parse_websocket_message(ws_msg2);
    
    ASSERT_NE(result1, nullptr);
    ASSERT_NE(result2, nullptr);
    
    // 不同消息应该生成不同的会话ID
    EXPECT_NE(result1->get_session_id(), result2->get_session_id());
}

TEST_F(MessageParserTest, SessionIdGeneration_CustomSessionId) {
    auto req = http_client_->create_login_request("user", "pass");
    std::string custom_session_id = "custom_session_12345";
    
    auto result = parser_->parse_http_request(req, custom_session_id);
    
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_session_id(), custom_session_id);
}

// ===== 统计信息测试 =====

TEST_F(MessageParserTest, ParserStats) {
    // 初始统计信息
    auto initial_stats = parser_->get_stats();
    
    // 处理一些HTTP请求
    auto req1 = http_client_->create_login_request("user1", "pass1");
    auto req2 = http_client_->create_register_request("user2", "pass2", "test@example.com");
    auto result1 = parser_->parse_http_request(req1);
    auto result2 = parser_->parse_http_request(req2);
    
    // 处理一些WebSocket消息
    auto ws_msg1 = websocket_client_->create_heartbeat_message();
    auto ws_msg2 = websocket_client_->create_chat_message("sender", "receiver", "test message");
    auto ws_result1 = parser_->parse_websocket_message(ws_msg1);
    auto ws_result2 = parser_->parse_websocket_message(ws_msg2);
    
    // 获取更新后的统计信息
    auto updated_stats = parser_->get_stats();
    
    // 验证统计信息更新
    EXPECT_EQ(updated_stats.http_requests_parsed, initial_stats.http_requests_parsed + 2);
    EXPECT_EQ(updated_stats.websocket_messages_parsed, initial_stats.websocket_messages_parsed + 2);
}

TEST_F(MessageParserTest, ParserStatsReset) {
    // 处理一些消息
    auto req = http_client_->create_login_request("user", "pass");
    auto result = parser_->parse_http_request(req);
    
    auto ws_msg = websocket_client_->create_heartbeat_message();
    auto ws_result = parser_->parse_websocket_message(ws_msg);
    
    // 获取统计信息
    auto stats_before_reset = parser_->get_stats();
    EXPECT_GT(stats_before_reset.http_requests_parsed, 0);
    EXPECT_GT(stats_before_reset.websocket_messages_parsed, 0);
    
    // 重置统计信息
    parser_->reset_stats();
    
    // 验证统计信息已重置
    auto stats_after_reset = parser_->get_stats();
    EXPECT_EQ(stats_after_reset.http_requests_parsed, 0);
    EXPECT_EQ(stats_after_reset.websocket_messages_parsed, 0);
}

// ===== 路由管理器集成测试 =====

TEST_F(MessageParserTest, RouterManagerIntegration) {
    // 获取路由管理器引用
    const auto& router_mgr = parser_->get_router_manager();
    
    // 验证路由管理器状态
    auto router_stats = router_mgr.get_stats();
    EXPECT_GT(router_stats.http_route_count, 0);
    EXPECT_GT(router_stats.service_count, 0);
}

TEST_F(MessageParserTest, ConfigReload) {
    // 尝试重新加载配置
    bool reload_result = parser_->reload_config();
    
    // 验证重新加载结果（应该成功，因为配置文件存在且有效）
    EXPECT_TRUE(reload_result);
}

// ===== 并发测试 =====

TEST_F(MessageParserTest, ConcurrentParsing) {
    const int num_threads = 4;
    const int messages_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // 创建多个线程同时解析消息
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &success_count, messages_per_thread, t]() {
            for (int i = 0; i < messages_per_thread; ++i) {
                // HTTP请求解析
                auto req = http_client_->create_login_request(
                    "user_" + std::to_string(t) + "_" + std::to_string(i),
                    "pass_" + std::to_string(i)
                );
                auto http_result = parser_->parse_http_request(req);
                if (http_result != nullptr) {
                    success_count.fetch_add(1);
                }
                
                // WebSocket消息解析
                auto ws_msg = websocket_client_->create_chat_message(
                    "sender_" + std::to_string(t),
                    "receiver_" + std::to_string(i),
                    "Message from thread " + std::to_string(t)
                );
                auto ws_result = parser_->parse_websocket_message(ws_msg);
                if (ws_result != nullptr) {
                    success_count.fetch_add(1);
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有消息都被成功解析
    EXPECT_EQ(success_count.load(), num_threads * messages_per_thread * 2);
}

// ===== 错误处理测试 =====

TEST_F(MessageParserTest, ErrorHandling_HttpRouteNotFound) {
    auto req = http_client_->create_invalid_request();
    auto result = parser_->parse_http_request(req);
    
    EXPECT_EQ(result, nullptr);
    
    // 验证统计信息中的失败计数增加
    auto stats = parser_->get_stats();
    EXPECT_GT(stats.routing_failures, 0);
}

TEST_F(MessageParserTest, ErrorHandling_WebSocketDecodeFailure) {
    auto invalid_msg = websocket_client_->create_invalid_message();
    auto result = parser_->parse_websocket_message(invalid_msg);
    
    EXPECT_EQ(result, nullptr);
    
    // 验证统计信息中的解码失败计数增加
    auto stats = parser_->get_stats();
    EXPECT_GT(stats.decode_failures, 0);
}

// ===== 性能测试 =====

TEST_F(MessageParserTest, PerformanceTest_HttpParsing) {
    const int num_requests = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_requests; ++i) {
        auto req = http_client_->create_login_request("user_" + std::to_string(i), "pass");
        auto result = parser_->parse_http_request(req);
        ASSERT_NE(result, nullptr);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Parsed " << num_requests << " HTTP requests in " 
              << duration.count() << "ms" << std::endl;
    std::cout << "Average: " << (duration.count() / static_cast<double>(num_requests)) 
              << "ms per request" << std::endl;
    
    // 验证性能要求（每个请求平均处理时间应该小于10ms）
    EXPECT_LT(duration.count() / static_cast<double>(num_requests), 10.0);
}

TEST_F(MessageParserTest, PerformanceTest_WebSocketParsing) {
    const int num_messages = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_messages; ++i) {
        auto msg = websocket_client_->create_heartbeat_message();
        auto result = parser_->parse_websocket_message(msg);
        ASSERT_NE(result, nullptr);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Parsed " << num_messages << " WebSocket messages in " 
              << duration.count() << "ms" << std::endl;
    std::cout << "Average: " << (duration.count() / static_cast<double>(num_messages)) 
              << "ms per message" << std::endl;
    
    // 验证性能要求（每个消息平均处理时间应该小于5ms）
    EXPECT_LT(duration.count() / static_cast<double>(num_messages), 5.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // 初始化日志系统
    LogManager::SetLogToConsole("global");
    LogManager::SetLoggingEnabled("global", true);
    
    return RUN_ALL_TESTS();
}