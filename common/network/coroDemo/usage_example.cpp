/******************************************************************************
 *
 * @file       usage_example.cpp
 * @brief      协程版本网络库使用示例和对比
 *
 * @author     myself
 * @date       2025/01/21
 *
 *****************************************************************************/

#include "coro_tcp_server.hpp"
#include "coro_tcp_session.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <string>

namespace im {
namespace network {
namespace examples {

using namespace coro;
namespace net = boost::asio;

// ============================================================================
// 1. 简单Echo服务器示例 - 协程版本
// ============================================================================

awaitable<void> run_echo_server() {
    net::io_context ioc;
    
    // 创建协程TCP服务器
    CoroTCPServer server(ioc, 8080);
    
    // 设置Echo处理器
    server.set_connection_handler(
        [](CoroTCPServer::SessionPtr session) -> awaitable<void> {
            std::cout << "New client connected from: " 
                      << session->remote_endpoint().address().to_string() << std::endl;
            
            // 设置消息处理器 - 简单的echo逻辑
            session->set_message_handler(
                [session](const std::string& message) -> awaitable<void> {
                    std::cout << "Received: " << message << std::endl;
                    
                    // Echo back with prefix
                    std::string response = "Echo: " + message;
                    co_await session->send(std::move(response));
                    
                    std::cout << "Sent: " << response << std::endl;
                }
            );
            
            // 设置断开连接处理器
            session->set_close_handler([]() {
                std::cout << "Client disconnected" << std::endl;
            });
            
            // 启动会话 - 开始处理这个连接
            co_await session->start();
        }
    );
    
    std::cout << "Echo server starting on port 8080..." << std::endl;
    
    // 启动服务器
    co_await server.start();
}

// ============================================================================
// 2. 聊天室服务器示例 - 展示广播功能
// ============================================================================

class ChatRoom {
public:
    static awaitable<void> run_chat_server() {
        net::io_context ioc;
        auto server = std::make_shared<CoroTCPServer>(ioc, 8081);
        
        server->set_connection_handler(
            [server](CoroTCPServer::SessionPtr session) -> awaitable<void> {
                std::cout << "User joined chat room from: " 
                          << session->remote_endpoint().address().to_string() << std::endl;
                
                // 欢迎消息
                co_await session->send("Welcome to the chat room! Type your messages...");
                
                // 设置消息处理器 - 广播到所有用户
                session->set_message_handler(
                    [server, session](const std::string& message) -> awaitable<void> {
                        std::string broadcast_msg = "[" + 
                            session->remote_endpoint().address().to_string() + 
                            "]: " + message;
                        
                        std::cout << "Broadcasting: " << broadcast_msg << std::endl;
                        
                        // 广播到所有连接的客户端
                        co_await server->broadcast(broadcast_msg);
                    }
                );
                
                session->set_close_handler([server]() {
                    std::cout << "User left chat room. Active users: " 
                              << server->session_count() << std::endl;
                });
                
                co_await session->start();
            }
        );
        
        std::cout << "Chat server starting on port 8081..." << std::endl;
        co_await server->start();
    }
};

// ============================================================================
// 3. 协程客户端示例
// ============================================================================

class CoroTCPClient {
public:
    explicit CoroTCPClient(net::io_context& ioc) : ioc_(ioc), socket_(ioc) {}
    
    awaitable<void> connect(const std::string& host, unsigned short port) {
        tcp::resolver resolver(ioc_);
        auto endpoints = co_await resolver.async_resolve(host, std::to_string(port), net::use_awaitable);
        
        co_await net::async_connect(socket_, endpoints, net::use_awaitable);
        
        std::cout << "Connected to " << host << ":" << port << std::endl;
    }
    
    awaitable<void> send_message(const std::string& message) {
        uint32_t length = htonl(static_cast<uint32_t>(message.size()));
        uint8_t msg_type = 0; // NORMAL
        
        std::vector<net::const_buffer> buffers;
        buffers.push_back(net::buffer(&length, sizeof(length)));
        buffers.push_back(net::buffer(&msg_type, sizeof(msg_type)));
        buffers.push_back(net::buffer(message));
        
        co_await net::async_write(socket_, buffers, net::use_awaitable);
        std::cout << "Sent: " << message << std::endl;
    }
    
    awaitable<std::string> receive_message() {
        // 读取消息头
        std::array<char, 5> header;
        co_await net::async_read(socket_, net::buffer(header), net::use_awaitable);
        
        uint32_t body_length;
        std::memcpy(&body_length, header.data(), sizeof(body_length));
        body_length = ntohl(body_length);
        
        if (body_length == 0) {
            co_return ""; // 心跳消息
        }
        
        // 读取消息体
        std::vector<char> buffer(body_length);
        co_await net::async_read(socket_, net::buffer(buffer), net::use_awaitable);
        
        std::string message(buffer.data(), body_length);
        std::cout << "Received: " << message << std::endl;
        co_return message;
    }

private:
    net::io_context& ioc_;
    tcp::socket socket_;
};

awaitable<void> run_client_example() {
    net::io_context ioc;
    CoroTCPClient client(ioc);
    
    try {
        // 连接到服务器
        co_await client.connect("127.0.0.1", 8080);
        
        // 发送几条测试消息
        co_await client.send_message("Hello Server!");
        co_await client.receive_message();
        
        co_await client.send_message("How are you?");
        co_await client.receive_message();
        
        co_await client.send_message("Goodbye!");
        co_await client.receive_message();
        
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

// ============================================================================
// 4. 主函数示例 - 演示如何运行不同的服务器
// ============================================================================

int main(int argc, char* argv[]) {
    try {
        net::io_context ioc;
        
        if (argc < 2) {
            std::cout << "Usage: " << argv[0] << " [echo|chat|client]" << std::endl;
            return 1;
        }
        
        std::string mode = argv[1];
        
        if (mode == "echo") {
            // 运行Echo服务器
            net::co_spawn(ioc, run_echo_server(), net::detached);
        } else if (mode == "chat") {
            // 运行聊天服务器
            net::co_spawn(ioc, ChatRoom::run_chat_server(), net::detached);
        } else if (mode == "client") {
            // 运行客户端示例
            net::co_spawn(ioc, run_client_example(), net::detached);
        } else {
            std::cout << "Unknown mode: " << mode << std::endl;
            return 1;
        }
        
        // 添加信号处理
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) {
            std::cout << "\nShutting down..." << std::endl;
            ioc.stop();
        });
        
        std::cout << "Starting event loop..." << std::endl;
        ioc.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

} // namespace examples
} // namespace network
} // namespace im

// ============================================================================
// 编译说明：
// g++ -std=c++20 -I/path/to/boost usage_example.cpp coro_tcp_session.cpp coro_tcp_server.cpp -lboost_system -pthread -o example
//
// 运行示例：
// ./example echo    # 启动Echo服务器
// ./example chat    # 启动聊天服务器  
// ./example client  # 运行客户端测试
// ============================================================================