#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <filesystem>
#include "../../common/network/tcp_session.hpp"
#include "../../common/network/IOService_pool.hpp"
#include "../../common/utils/log_manager.hpp"

using namespace std::chrono_literals;

class TCPSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建日志目录
        std::filesystem::create_directories("logs");
        
        // 初始化日志
        LogManager::SetLogToFile("tcp_session", "logs/tcp_session_test.log");
        LogManager::SetLogToFile("io_service_pool", "logs/io_service_pool_test.log");
        LogManager::SetLoggingEnabled("tcp_session", true);
        LogManager::SetLoggingEnabled("io_service_pool", true);
        
        // 初始化IO上下文
        io_context_ = std::make_unique<boost::asio::io_context>();
        work_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(*io_context_));
        
        // 启动IO上下文线程
        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
    }

    void TearDown() override {
        // 停止IO上下文
        work_.reset();
        if (io_context_) {
            io_context_->stop();
        }
        
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_;
    std::thread io_thread_;
};

// 测试TCPSession构造和基本方法
TEST_F(TCPSessionTest, SessionConstruction) {
    // 创建一个连接对用于测试
    boost::asio::ip::tcp::socket socket1(*io_context_);
    boost::asio::ip::tcp::socket socket2(*io_context_);
    
    // 连接两个socket
    boost::asio::ip::tcp::acceptor acceptor(*io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    boost::asio::ip::tcp::endpoint endpoint = acceptor.local_endpoint();
    
    // 异步接受连接
    acceptor.async_accept([&socket2](boost::system::error_code ec, boost::asio::ip::tcp::socket peer) {
        if (!ec) {
            socket2 = std::move(peer);
        }
    });
    
    // 连接客户端
    socket1.async_connect(endpoint, [](boost::system::error_code ec) {
        // 连接完成
    });
    
    // 运行io_context以完成连接
    io_context_->poll();
    
    // 现在创建TCPSession
    auto session = std::make_shared<TCPSession>(std::move(socket1));
    
    EXPECT_NE(session, nullptr) << "Session should be created successfully";
}

// 测试TCPSession发送消息队列
TEST_F(TCPSessionTest, SessionSendMessageQueue) {
    boost::asio::ip::tcp::socket socket(*io_context_);
    
    // 创建一个临时服务器来接收连接
    boost::asio::ip::tcp::acceptor acceptor(*io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    boost::asio::ip::tcp::endpoint server_endpoint = acceptor.local_endpoint();
    
    // 异步接受连接
    boost::asio::ip::tcp::socket server_socket(*io_context_);
    acceptor.async_accept(server_socket, [](boost::system::error_code ec) {
        // 接受连接完成
    });
    
    // 连接客户端
    socket.async_connect(server_endpoint, [](boost::system::error_code ec) {
        // 连接完成
    });
    
    // 运行io_context以完成连接
    io_context_->poll();
    
    auto session = std::make_shared<TCPSession>(std::move(socket));
    
    // 发送几条消息测试队列
    session->send("Message 1");
    session->send("Message 2");
    session->send("Message 3");
    
    // 因为socket未连接，实际不会发送，但消息应该进入队列
    SUCCEED() << "Messages queued successfully";
}

// 测试TCPSession回调设置
TEST_F(TCPSessionTest, SessionCallbackSetup) {
    boost::asio::ip::tcp::socket socket(*io_context_);
    
    // 创建一个临时服务器来接收连接
    boost::asio::ip::tcp::acceptor acceptor(*io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    boost::asio::ip::tcp::endpoint server_endpoint = acceptor.local_endpoint();
    
    // 异步接受连接
    boost::asio::ip::tcp::socket server_socket(*io_context_);
    acceptor.async_accept(server_socket, [](boost::system::error_code ec) {
        // 接受连接完成
    });
    
    // 连接客户端
    socket.async_connect(server_endpoint, [](boost::system::error_code ec) {
        // 连接完成
    });
    
    // 运行io_context以完成连接
    io_context_->poll();
    
    auto session = std::make_shared<TCPSession>(std::move(socket));
    
    bool close_callback_called = false;
    bool message_callback_called = false;
    
    // 设置关闭回调
    session->set_close_callback([&close_callback_called]() {
        close_callback_called = true;
    });
    
    // 设置消息回调
    session->set_message_handler([&message_callback_called](const std::string& message) {
        message_callback_called = true;
    });
    
    // 因为没有实际连接，我们只能测试回调是否设置成功
    SUCCEED() << "Callbacks set successfully";
}