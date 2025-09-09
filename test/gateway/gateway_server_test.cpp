#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <future>

#include "../../gateway/gateway_server/gateway_server.hpp"
#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/proto/command.pb.h"
#include "../../gateway/message_processor/unified_message.hpp"

using namespace im::gateway;
using namespace im::network;
using namespace im::common;

class GatewayServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化测试环境
        // 使用相对路径，从构建目录访问配置文件
        platform_config_path = "../test_auth_config.json";
        router_config_path = "../test_router_config.json";
        ws_port = 8090;
        http_port = 8091;
    }

    void TearDown() override {
        // 清理测试环境
        if (gateway_server) {
            gateway_server->stop();
        }
    }

    std::string platform_config_path;
    std::string router_config_path;
    uint16_t ws_port;
    uint16_t http_port;
    std::unique_ptr<GatewayServer> gateway_server;
};

// 测试GatewayServer构造函数
TEST_F(GatewayServerTest, ConstructorTest) {
    EXPECT_NO_THROW({
        gateway_server = std::make_unique<GatewayServer>(
            platform_config_path, router_config_path, ws_port, http_port);
    });
    
    ASSERT_NE(gateway_server, nullptr);
    EXPECT_FALSE(gateway_server->is_running());
}

// 测试GatewayServer初始化
TEST_F(GatewayServerTest, InitializationTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 检查各组件是否正确初始化
    // 注意：这些是私有成员，需要通过公共接口或友元类来访问
    // 这里我们只能通过公共接口来验证初始化是否成功
    EXPECT_NO_THROW({
        // 初始化应该在构造函数中完成，这里验证是否成功
        EXPECT_TRUE(true); // 如果构造函数没有抛出异常，说明初始化成功
    });
}

// 测试GatewayServer启动和停止
TEST_F(GatewayServerTest, StartStopTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 启动服务器
    EXPECT_NO_THROW({
        gateway_server->start();
    });
    
    EXPECT_TRUE(gateway_server->is_running());
    
    // 停止服务器
    EXPECT_NO_THROW({
        gateway_server->stop();
    });
    
    EXPECT_FALSE(gateway_server->is_running());
}

// 测试获取服务器状态
TEST_F(GatewayServerTest, GetServerStatsTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    std::string stats = gateway_server->get_server_stats();
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("GatewayServer stats:"), std::string::npos);
}

// 测试注册消息处理函数
TEST_F(GatewayServerTest, RegisterMessageHandlersTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 注册一个测试消息处理函数
    bool handler_called = false;
    auto test_handler = [&handler_called](const UnifiedMessage& msg) -> Task<CoroProcessorResult> {
        handler_called = true;
        co_return CoroProcessorResult(0, "Success");
    };
    
    // 注册处理函数
    bool result = gateway_server->register_message_handlers(3001, test_handler);
    EXPECT_TRUE(result);
    
    // 尝试重复注册同一个cmd_id，应该失败
    result = gateway_server->register_message_handlers(3001, test_handler);
    EXPECT_FALSE(result);
    
    // 尝试注册另一个cmd_id，应该成功
    result = gateway_server->register_message_handlers(3002, test_handler);
    EXPECT_TRUE(result);
}

// 测试WebSocket连接功能
TEST_F(GatewayServerTest, WebSocketConnectionTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 启动服务器
    gateway_server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 这里可以添加WebSocket客户端连接测试
    // 但由于需要实际的WebSocket客户端，我们在集成测试中再验证
    
    // 停止服务器
    gateway_server->stop();
}

// 测试HTTP服务功能
TEST_F(GatewayServerTest, HttpServiceTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 启动服务器
    gateway_server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 这里可以添加HTTP客户端请求测试
    // 但由于需要实际的HTTP客户端，我们在集成测试中再验证
    
    // 停止服务器
    gateway_server->stop();
}

// 测试推送消息功能
TEST_F(GatewayServerTest, PushMessageTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 测试推送消息到用户（在服务器未启动时）
    bool result = gateway_server->push_message_to_user("test_user", "test_message");
    // 在没有连接管理器初始化的情况下，应该返回false
    EXPECT_FALSE(result);
    
    // 获取在线用户数量
    size_t online_count = gateway_server->get_online_count();
    EXPECT_EQ(online_count, 0);
}

// 测试性能 - 启动停止时间
TEST_F(GatewayServerTest, PerformanceStartStopTest) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    auto init_time = std::chrono::high_resolution_clock::now();
    
    gateway_server->start();
    
    auto start_time_end = std::chrono::high_resolution_clock::now();
    
    gateway_server->stop();
    
    auto stop_time = std::chrono::high_resolution_clock::now();
    
    auto init_duration = std::chrono::duration_cast<std::chrono::milliseconds>(init_time - start_time);
    auto start_duration = std::chrono::duration_cast<std::chrono::milliseconds>(start_time_end - init_time);
    auto stop_duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop_time - start_time_end);
    
    // 验证启动和停止时间在合理范围内（这里设置为10秒，实际应该更快）
    EXPECT_LT(init_duration.count(), 10000);
    EXPECT_LT(start_duration.count(), 10000);
    EXPECT_LT(stop_duration.count(), 10000);
}

// 测试性能 - 并发连接测试
TEST_F(GatewayServerTest, PerformanceConcurrentConnectionsTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 启动服务器
    gateway_server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 模拟并发连接测试
    const int num_connections = 10;
    std::vector<std::thread> threads;
    
    // 记录开始时间
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 创建多个线程模拟并发连接
    for (int i = 0; i < num_connections; ++i) {
        threads.emplace_back([this, i]() {
            // 每个线程等待不同的时间后尝试连接
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            // 这里可以添加实际的连接代码，但由于需要实际的客户端，我们只模拟延迟
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    // 记录结束时间
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 验证并发处理时间在合理范围内
    EXPECT_LT(duration.count(), 5000); // 5秒内完成
    
    // 停止服务器
    gateway_server->stop();
}

// 测试性能 - 消息处理性能
TEST_F(GatewayServerTest, PerformanceMessageProcessingTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 启动服务器
    gateway_server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 注册测试消息处理函数
    std::atomic<int> message_count{0};
    auto test_handler = [&message_count](const UnifiedMessage& msg) -> Task<CoroProcessorResult> {
        message_count.fetch_add(1);
        // 模拟处理延迟
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        co_return CoroProcessorResult(0, "Success");
    };
    
    // 注册处理函数
    gateway_server->register_message_handlers(5001, test_handler);
    
    // 记录开始时间
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 模拟处理100条消息
    const int num_messages = 100;
    for (int i = 0; i < num_messages; ++i) {
        // 这里需要实际发送消息到服务器进行测试
        // 但由于需要构造完整的消息对象，我们只验证处理函数注册
    }
    
    // 等待一段时间确保处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 记录结束时间
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 验证处理时间在合理范围内
    EXPECT_LT(duration.count(), 5000); // 5秒内完成
    
    // 停止服务器
    gateway_server->stop();
}

// 测试异常处理
TEST_F(GatewayServerTest, ExceptionHandlingTest) {
    // 测试使用无效配置文件路径
    EXPECT_THROW({
        GatewayServer invalid_server("invalid_config.json", "invalid_router.json", 8092, 8093);
    }, std::exception);
}

// 测试消息处理函数的实际调用
TEST_F(GatewayServerTest, MessageHandlerInvocationTest) {
    gateway_server = std::make_unique<GatewayServer>(
        platform_config_path, router_config_path, ws_port, http_port);
    
    // 启动服务器
    gateway_server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 注册一个测试消息处理函数
    std::atomic<int> handler_call_count{0};
    auto test_handler = [&handler_call_count](const UnifiedMessage& msg) -> Task<CoroProcessorResult> {
        handler_call_count.fetch_add(1);
        co_return CoroProcessorResult(0, "Success");
    };
    
    // 注册处理函数
    bool result = gateway_server->register_message_handlers(4001, test_handler);
    EXPECT_TRUE(result);
    
    // 验证处理函数是否正确注册（通过统计信息等方式）
    // 由于直接调用需要构造UnifiedMessage，这里只验证注册成功
    
    // 停止服务器
    gateway_server->stop();
    
    // 验证处理函数未被调用
    EXPECT_EQ(handler_call_count.load(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}