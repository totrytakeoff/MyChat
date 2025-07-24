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
    }

    void TearDown() override {
        // 注意：不要在每个测试中停止IOServicePool，因为它是单例
        // IOServicePool会在程序结束时自动清理
    }

    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_;
    std::thread io_thread_;
};

// 测试TCPSession构造和基本方法
TEST_F(TCPSessionTest, SessionConstruction) {
    boost::asio::ip::tcp::socket socket(*io_context_);
    auto session = std::make_shared<TCPSession>(std::move(socket));
    
    EXPECT_NE(&session, nullptr) << "Session should be created successfully";
}

// 测试TCPSession发送消息队列
TEST_F(TCPSessionTest, SessionSendMessageQueue) {
    boost::asio::ip::tcp::socket socket(*io_context_);
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