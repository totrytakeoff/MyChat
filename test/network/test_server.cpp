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
        std::cout << "Connection accepted, count: " << connection_count.load() << std::endl;
        
        // 设置会话消息处理回调
        session->set_message_handler([](const std::string& message) {
            std::cout << "Received message: " << message << std::endl;
        });
        
        // 启动会话
        session->start();
        
        // 通知连接已建立
        if (connection_count == 1) {
            std::cout << "Setting promise value" << std::endl;
            connection_established.set_value();
        }
    });
    
    // 启动服务器
    server->start();
    std::this_thread::sleep_for(50ms); // 确保服务器启动完成
    
    // 创建客户端连接
    boost::asio::io_context client_io_context;
    boost::asio::ip::tcp::socket socket(client_io_context);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), test_port);
    
    std::cout << "Connecting to server..." << std::endl;
    boost::system::error_code ec;
    socket.connect(endpoint, ec);
    EXPECT_FALSE(ec) << "Failed to connect to server: " << ec.message();
    
    std::thread client_thread;
    if (!ec) {
        std::cout << "Connected to server" << std::endl;
        
        // 启动客户端io_context的运行循环
        client_thread = std::thread([&client_io_context]() {
            client_io_context.run();
        });
        
        // 等待连接被服务器接受
        std::cout << "Waiting for connection establishment..." << std::endl;
        auto wait_result = connection_future.wait_for(5s);
        EXPECT_EQ(wait_result, std::future_status::ready) << "Connection should be established within timeout";
        
        EXPECT_EQ(connection_count, 1) << "Server should have accepted exactly one connection";
        
        // 关闭客户端连接
        std::cout << "Closing socket..." << std::endl;
        socket.close();
        
        // 等待一小段时间确保连接关闭被处理
        std::this_thread::sleep_for(100ms);
    }
    
    // 停止客户端io_context
    std::cout << "Stopping client io_context..." << std::endl;
    client_io_context.stop();
    if (client_thread.joinable()) {
        std::cout << "Joining client thread..." << std::endl;
        client_thread.join();
    }
    
    // 停止服务器
    std::cout << "Stopping server..." << std::endl;
    server->stop();
    std::cout << "Server stopped" << std::endl;
}

// 测试TCPSession能否发送和接收消息
TEST_F(TCPServerTest, SessionSendMessage) {
    const unsigned short test_port = 20003;
    std::atomic<bool> message_received{false};
    std::string received_message;
    std::promise<void> message_received_promise;
    std::future<void> message_received_future = message_received_promise.get_future();
    
    // 创建服务器
    auto server = std::make_unique<TCPServer>(test_port);
    
    // 保存会话指针，以便后续发送消息
    TCPSession::Ptr server_session;
    
    // 设置连接回调
    server->set_connection_handler([&message_received, &received_message, &message_received_promise, &server_session](TCPSession::Ptr session) {
        server_session = session;
        std::cout << "Server: Connection accepted and session created" << std::endl;
        
        // 设置会话消息处理回调
        session->set_message_handler([&message_received, &received_message, &message_received_promise](const std::string& message) {
            std::cout << "Server received message: " << message << std::endl;
            received_message = message;
            message_received = true;
            message_received_promise.set_value(); // 通知消息已接收
        });
        
        // 注意：不要在这里调用session->start()，因为TCPServer会在handle_accept中自动调用
        std::cout << "Server: Session created and callbacks set" << std::endl;
    });
    
    // 启动服务器
    std::cout << "Server: Starting server" << std::endl;
    server->start();
    
    // 等待服务器启动
    std::this_thread::sleep_for(100ms);
    
    // 创建客户端连接并发送消息
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), test_port);
    
    boost::system::error_code ec;
    std::cout << "Client: Connecting to server..." << std::endl;
    socket.connect(endpoint, ec);
    
    EXPECT_FALSE(ec) << "Failed to connect to server: " << ec.message();
    
    std::thread client_thread;
    if (!ec) {
        std::cout << "Client: Connected to server" << std::endl;
        
        // 启动io_context的运行循环
        client_thread = std::thread([&io_context]() {
            std::cout << "Client: Starting io_context run loop" << std::endl;
            io_context.run();
            std::cout << "Client: io_context run loop finished" << std::endl;
        });
        
        // 等待一小段时间确保连接完全建立
        std::this_thread::sleep_for(50ms);
        
        // 发送测试消息 (带长度前缀)
        std::string test_message = "Hello, TCPServer!";
        uint32_t length = htonl(static_cast<uint32_t>(test_message.size())); // 使用4字节长度前缀
        
        std::cout << "Client: Sending message..." << std::endl;
        // 发送长度
        boost::asio::write(socket, boost::asio::buffer(&length, sizeof(length)), ec);
        EXPECT_FALSE(ec) << "Failed to send message length: " << ec.message();
        std::cout << "Client: Sent length, ec=" << ec.message() << std::endl;
        
        // 发送消息体
        if (!ec) {
            boost::asio::write(socket, boost::asio::buffer(test_message), ec);
            EXPECT_FALSE(ec) << "Failed to send message body: " << ec.message();
            std::cout << "Client: Sent body, ec=" << ec.message() << std::endl;
        }
        
        // 等待消息被处理
        std::cout << "Client: Waiting for message to be received..." << std::endl;
        auto wait_result = message_received_future.wait_for(2s); // 等待消息接收完成
        std::cout << "Client: Wait result: " << (wait_result == std::future_status::ready ? "ready" : "timeout") << std::endl;
        EXPECT_EQ(wait_result, std::future_status::ready) << "Message should be received within timeout";
        
        EXPECT_TRUE(message_received) << "Server should have received the message";
        if (message_received) {
            EXPECT_EQ(received_message, test_message) << "Received message should match sent message";
        }
        
        // 现在测试服务器向客户端发送消息
        std::atomic<bool> response_received{false};
        std::string response_message;
        std::promise<void> response_received_promise;
        std::future<void> response_received_future = response_received_promise.get_future();
        
        if (server_session && message_received) {
            std::cout << "Client: Sending response from server to client" << std::endl;
            // 服务器发送消息给客户端
            std::string server_response = "Hello, Client!";
            server_session->send(server_response);
            
            // 客户端读取服务器发送的消息
            std::array<char, 4> header_buffer;
            boost::system::error_code read_ec;
            
            // 读取4字节长度字段
            std::cout << "Client: Reading header..." << std::endl;
            size_t header_bytes_read = boost::asio::read(socket, boost::asio::buffer(header_buffer), read_ec);
            std::cout << "Client: Header read, bytes=" << header_bytes_read << ", ec=" << read_ec.message() << std::endl;
            
            if (!read_ec && header_bytes_read == 4) {
                // 解析长度字段（网络字节序转为主机字节序）
                uint32_t body_length = ntohl(*reinterpret_cast<uint32_t*>(header_buffer.data()));
                std::cout << "Client: Body length=" << body_length << std::endl;
                
                // 读取消息体
                std::vector<char> body_buffer(static_cast<size_t>(body_length));
                size_t body_bytes_read = boost::asio::read(socket, boost::asio::buffer(body_buffer), read_ec);
                std::cout << "Client: Body read, bytes=" << body_bytes_read << ", ec=" << read_ec.message() << std::endl;
                
                if (!read_ec && body_bytes_read == body_length) {
                    response_message = std::string(body_buffer.data(), body_length);
                    response_received = true;
                    response_received_promise.set_value();
                }
                else {
                    FAIL() << "Failed to read message body: " << read_ec.message();
                }
            }
            else {
                FAIL() << "Failed to read header: " << read_ec.message();
            }
            
            // 等待响应消息被接收
            std::cout << "Client: Waiting for response message..." << std::endl;
            auto response_wait_result = response_received_future.wait_for(2s);
            std::cout << "Client: Response wait result: " << (response_wait_result == std::future_status::ready ? "ready" : "timeout") << std::endl;
            EXPECT_EQ(response_wait_result, std::future_status::ready) << "Response message should be received within timeout";
            
            EXPECT_TRUE(response_received) << "Client should have received the response message";
            if (response_received) {
                EXPECT_EQ(response_message, server_response) << "Response message should match sent message";
            }
        }
        
        // 关闭客户端连接
        std::cout << "Client: Closing socket..." << std::endl;
        socket.close();
        
        // 等待一小段时间确保连接关闭被处理
        std::this_thread::sleep_for(100ms);
    }
    
    // 停止io_context
    std::cout << "Client: Stopping io_context..." << std::endl;
    io_context.stop();
    if (client_thread.joinable()) {
        std::cout << "Client: Joining client thread..." << std::endl;
        client_thread.join();
    }
    
    // 停止服务器
    std::cout << "Server: Stopping server..." << std::endl;
    server->stop();
    std::cout << "Server: Server stopped" << std::endl;
}
