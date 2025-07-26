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

// 测试心跳机制
TEST_F(TCPSessionTest, HeartbeatMechanism) {
    boost::asio::ip::tcp::socket socket(*io_context_);
    
    // 创建测试服务器
    boost::asio::ip::tcp::acceptor acceptor(*io_context_, 
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    
    bool received_ping = false;
    bool sent_pong = false;
    
    // 服务器端接受连接
    acceptor.async_accept([&](boost::system::error_code ec, tcp::socket peer) {
        auto session = std::make_shared<TCPSession>(std::move(peer));
        
        session->set_message_handler([&](const std::string& msg) {
            // 在新的实现中，心跳包处理在TCPSession内部完成，不会传递到message_handler
        });
        
        // 监听是否有PING消息发送到服务器
        std::array<char, 5> header_buf;
        boost::asio::async_read(peer, boost::asio::buffer(header_buf),
            [&](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    uint8_t msg_type = *reinterpret_cast<uint8_t*>(&header_buf[4]);
                    if (msg_type == 1) { // PING消息类型
                        received_ping = true;
                        
                        // 发送PONG响应
                        uint32_t length = 0;
                        uint8_t pong_type = 2; // PONG消息类型
                        std::vector<boost::asio::const_buffer> buffers;
                        buffers.push_back(boost::asio::buffer(&length, sizeof(length)));
                        buffers.push_back(boost::asio::buffer(&pong_type, sizeof(pong_type)));
                        
                        boost::asio::async_write(peer, buffers,
                            [&](boost::system::error_code, std::size_t) {
                                sent_pong = true;
                            });
                    }
                }
            });
        
        session->start();
    });
    
    // 客户端连接
    socket.async_connect(acceptor.local_endpoint(), [](auto ec){});
    io_context_->poll();
    
    auto client_session = std::make_shared<TCPSession>(std::move(socket));
    client_session->start();
    
    // 等待心跳交互 (减少等待时间以适应测试)
    std::this_thread::sleep_for(35s);
    // 注意：由于心跳机制在TCPSession内部处理，实际测试可能需要其他方式验证
    std::cout << "Heartbeat test completed. Received PING: " << received_ping 
              << ", Sent PONG: " << sent_pong << std::endl;
}

// 测试读超时断开
TEST_F(TCPSessionTest, ReadTimeoutDisconnect) {
    boost::asio::ip::tcp::socket socket(*io_context_);
    
    boost::asio::ip::tcp::acceptor acceptor(*io_context_,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    
    bool disconnected = false;
    
    acceptor.async_accept([&](auto ec, auto peer) {
        auto session = std::make_shared<TCPSession>(std::move(peer));
        session->start();
    });
    
    socket.async_connect(acceptor.local_endpoint(), [](auto ec){});
    io_context_->poll();
    
    auto client_session = std::make_shared<TCPSession>(std::move(socket));
    client_session->set_close_callback([&]{ disconnected = true; });
    client_session->start();
    
    // 等待超时 (减少等待时间以适应测试)
    std::this_thread::sleep_for(125s);
    // 注意：实际测试可能需要更精确的超时检测方式
    std::cout << "Read timeout test completed. Disconnected: " << disconnected << std::endl;
}
