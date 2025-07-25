#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <filesystem>
#include "../../common/network/tcp_server.hpp"
#include "../../common/network/tcp_session.hpp"
#include "../../common/network/IOService_pool.hpp"
#include "../../common/utils/log_manager.hpp"

using namespace std::chrono_literals;
using tcp = boost::asio::ip::tcp;

class SimpleTCPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建日志目录
        std::filesystem::create_directories("logs");
        
        // 初始化日志
        LogManager::SetLogToFile("tcp_server", "logs/tcp_server_test.log");
        LogManager::SetLogToFile("tcp_session", "logs/tcp_session_test.log");
        LogManager::SetLogToFile("io_service_pool", "logs/io_service_pool_test.log");
        LogManager::SetLoggingEnabled("tcp_server", true);
        LogManager::SetLoggingEnabled("tcp_session", true);
        LogManager::SetLoggingEnabled("io_service_pool", true);
    }

    void TearDown() override {
        // 注意：不要在每个测试中停止IOServicePool，因为它是单例
        // IOServicePool会在程序结束时自动清理
    }
};

// 最简单的服务器测试
TEST_F(SimpleTCPServerTest, BasicServerStartStop) {
    const unsigned short test_port = 20101;
    
    std::cout << "Creating server on port " << test_port << std::endl;
    auto server = std::make_unique<TCPServer>(test_port);
    
    std::cout << "Starting server" << std::endl;
    server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(100ms);
    
    std::cout << "Stopping server" << std::endl;
    server->stop();
    
    SUCCEED() << "Basic server start/stop test passed";
}

// 简化的连接测试
TEST_F(SimpleTCPServerTest, SimpleConnectionTest) {
    const unsigned short test_port = 20102;
    std::atomic<int> connection_count{0};
    
    std::cout << "Creating server on port " << test_port << std::endl;
    auto server = std::make_unique<TCPServer>(test_port);
    
    // 设置连接回调
    server->set_connection_handler([&connection_count](TCPSession::Ptr session) {
        connection_count++;
        std::cout << "Server accepted connection #" << connection_count.load() << std::endl;
        
        // 设置简单的消息处理回调
        session->set_message_handler([](const std::string& message) {
            std::cout << "Server received message: " << message << std::endl;
        });
        
        // 启动会话
        session->start();
    });
    
    std::cout << "Starting server" << std::endl;
    server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(50ms);
    
    // 创建客户端连接
    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    tcp::endpoint endpoint(tcp::v4(), test_port);
    
    std::cout << "Client connecting to server" << std::endl;
    boost::system::error_code ec;
    socket.connect(endpoint, ec);
    
    std::thread io_thread;
    if (!ec) {
        ASSERT_FALSE(ec) << "Failed to connect to server: " << ec.message();
        std::cout << "Client connected successfully" << std::endl;
        
        // 启动io_context运行循环
        io_thread = std::thread([&io_context]() {
            std::cout << "Starting io_context run loop" << std::endl;
            io_context.run();
            std::cout << "io_context run loop finished" << std::endl;
        });
        
        // 等待一小段时间确保连接被处理
        std::this_thread::sleep_for(100ms);
        
        EXPECT_EQ(connection_count, 1) << "Server should have accepted exactly one connection";
        
        std::cout << "Closing client socket" << std::endl;
        socket.close();
        
        // 等待一小段时间确保连接关闭被处理
        std::this_thread::sleep_for(100ms);
    }
    
    // 停止io_context
    std::cout << "Stopping io_context" << std::endl;
    io_context.stop();
    if (io_thread.joinable()) {
        std::cout << "Joining io_thread" << std::endl;
        io_thread.join();
    }
    
    std::cout << "Stopping server" << std::endl;
    server->stop();
    
    std::cout << "Test completed" << std::endl;
    SUCCEED() << "Simple connection test completed";
}