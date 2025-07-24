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

class TCPServerTest : public ::testing::Test {
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

// 测试TCPServer能否正常启动和停止
TEST_F(TCPServerTest, ServerStartAndStop) {
    const unsigned short test_port = 20001;
    
    // 创建服务器
    auto server = std::make_unique<TCPServer>(test_port);
    
    // 启动服务器
    server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(100ms);
    
    // 停止服务器
    server->stop();
    
    SUCCEED() << "Server started on port " << test_port << " and stopped successfully";
}

// 测试TCPServer能否接受连接
TEST_F(TCPServerTest, ServerAcceptConnection) {
    const unsigned short test_port = 20002;
    std::atomic<int> connection_count{0};
    std::promise<void> connection_established;
    std::future<void> connection_future = connection_established.get_future();
    
    // 创建服务器
    auto server = std::make_unique<TCPServer>(test_port);
    
    // 设置连接回调
    server->set_connection_handler([&](TCPSession::Ptr session) {
        connection_count++;
        // 设置会话消息处理回调
        session->set_message_handler([](const std::string& message) {
            // Echo消息
            // 注意：实际测试中可能不需要回复消息
        });
        
        // 启动会话
        session->start();
        
        // 通知连接已建立
        if (connection_count == 1) {
            connection_established.set_value();
        }
    });
    
    // 启动服务器
    server->start();
    
    // 创建客户端连接
    boost::asio::io_context client_io_context;
    boost::asio::ip::tcp::socket socket(client_io_context);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), test_port);
    
    boost::system::error_code ec;
    socket.connect(endpoint, ec);
    EXPECT_FALSE(ec) << "Failed to connect to server: " << ec.message();
    
    if (!ec) {
        // 启动客户端io_context的运行循环
        std::thread client_thread([&client_io_context]() {
            client_io_context.run();
        });
        
        // 等待连接被服务器接受
        auto wait_result = connection_future.wait_for(5s);
        EXPECT_EQ(wait_result, std::future_status::ready) << "Connection should be established within timeout";
        
        EXPECT_EQ(connection_count, 1) << "Server should have accepted exactly one connection";
        
        // 关闭客户端连接
        socket.close();
        
        // 停止客户端io_context
        client_io_context.stop();
        if (client_thread.joinable()) {
            client_thread.join();
        }
    }
    
    // 停止服务器
    server->stop();
}

// 测试TCPSession能否发送和接收消息
TEST_F(TCPServerTest, SessionSendMessage) {
    const unsigned short test_port = 20003;
    std::atomic<bool> message_received{false};
    std::string received_message;
    
    // 创建服务器
    auto server = std::make_unique<TCPServer>(test_port);
    
    // 设置连接回调
    server->set_connection_handler([&message_received, &received_message](TCPSession::Ptr session) {
        // 设置会话消息处理回调
        session->set_message_handler([&message_received, &received_message](const std::string& message) {
            received_message = message;
            message_received = true;
        });
        
        // 启动会话
        session->start();
    });
    
    // 启动服务器
    server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(100ms);
    
    // 创建客户端连接并发送消息
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), test_port);
    
    boost::system::error_code ec;
    socket.connect(endpoint, ec);
    
    EXPECT_FALSE(ec) << "Failed to connect to server: " << ec.message();
    
    if (!ec) {
        // 启动io_context的运行循环
        std::thread client_thread([&io_context]() {
            io_context.run();
        });
        
        // 发送测试消息 (带长度前缀)
        std::string test_message = "Hello, TCPServer!";
        uint32_t length = htonl(static_cast<uint32_t>(test_message.size()));
        
        // 发送长度
        boost::asio::write(socket, boost::asio::buffer(&length, sizeof(length)), ec);
        EXPECT_FALSE(ec) << "Failed to send message length: " << ec.message();
        
        // 发送消息体
        if (!ec) {
            boost::asio::write(socket, boost::asio::buffer(test_message), ec);
            EXPECT_FALSE(ec) << "Failed to send message body: " << ec.message();
        }
        
        // 等待消息被处理
        for (int i = 0; i < 10 && !message_received; ++i) {
            std::this_thread::sleep_for(50ms);
        }
        
        EXPECT_TRUE(message_received) << "Server should have received the message";
        if (message_received) {
            EXPECT_EQ(received_message, test_message) << "Received message should match sent message";
        }
        
        // 关闭客户端连接
        socket.close();
        
        // 停止io_context
        io_context.stop();
        if (client_thread.joinable()) {
            client_thread.join();
        }
    }
    
    // 停止服务器
    server->stop();
}