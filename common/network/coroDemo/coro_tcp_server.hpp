#ifndef CORO_TCP_SERVER_HPP
#define CORO_TCP_SERVER_HPP

/******************************************************************************
 *
 * @file       coro_tcp_server.hpp
 * @brief      基于C++20协程的TCP服务器 - 示例实现
 *
 * @author     myself
 * @date       2025/01/21
 *
 *****************************************************************************/

#include "coro_tcp_session.hpp"
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <mutex>

namespace im {
namespace network {
namespace coro {

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace boost::asio::experimental::awaitable_operators;

/**
 * @class CoroTCPServer
 * @brief 基于协程的TCP服务器
 * 
 * 特点：
 * 1. 使用协程简化异步编程
 * 2. 每个连接一个协程，资源利用率高
 * 3. 统一的错误处理和资源管理
 * 4. 支持优雅关闭
 */
class CoroTCPServer {
public:
    using SessionPtr = CoroTCPSession::Ptr;
    using ConnectionHandler = std::function<awaitable<void>(SessionPtr)>;

    explicit CoroTCPServer(net::io_context& ioc, unsigned short port);
    ~CoroTCPServer();

    // 启动服务器
    awaitable<void> start();
    
    // 停止服务器
    awaitable<void> stop();
    
    // 设置连接处理器
    void set_connection_handler(ConnectionHandler handler) {
        connection_handler_ = std::move(handler);
    }
    
    // 获取活动会话数量
    size_t session_count() const {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        return sessions_.size();
    }
    
    // 广播消息到所有会话
    awaitable<void> broadcast(const std::string& message);

private:
    // 接受连接的协程
    awaitable<void> accept_loop();
    
    // 处理单个会话
    awaitable<void> handle_session(SessionPtr session);
    
    // 默认连接处理器
    awaitable<void> default_connection_handler(SessionPtr session);
    
    // 会话管理
    void add_session(SessionPtr session);
    void remove_session(SessionPtr session);

private:
    net::io_context& io_context_;
    tcp::acceptor acceptor_;
    
    // 会话管理
    mutable std::mutex sessions_mutex_;
    std::set<SessionPtr> sessions_;
    
    // 状态控制
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_stopping_{false};
    
    // 连接处理器
    ConnectionHandler connection_handler_;
};

/**
 * @brief 协程服务器工厂类
 */
class CoroServerFactory {
public:
    // 创建TCP服务器并运行
    static awaitable<void> run_tcp_server(
        net::io_context& ioc, 
        unsigned short port,
        CoroTCPServer::ConnectionHandler handler = nullptr) {
        
        CoroTCPServer server(ioc, port);
        if (handler) {
            server.set_connection_handler(std::move(handler));
        }
        
        co_await server.start();
    }
    
    // 创建echo服务器示例
    static CoroTCPServer::ConnectionHandler create_echo_handler() {
        return [](CoroTCPServer::SessionPtr session) -> awaitable<void> {
            session->set_message_handler(
                [session](const std::string& message) -> awaitable<void> {
                    // 简单的echo逻辑
                    std::string response = "Echo: " + message;
                    co_await session->send(std::move(response));
                }
            );
            
            // 启动会话
            co_await session->start();
        };
    }
    
    // 创建聊天服务器示例
    static CoroTCPServer::ConnectionHandler create_chat_handler(
        std::shared_ptr<CoroTCPServer> server) {
        return [server](CoroTCPServer::SessionPtr session) -> awaitable<void> {
            session->set_message_handler(
                [server, session](const std::string& message) -> awaitable<void> {
                    // 广播消息到所有客户端
                    std::string broadcast_msg = "User: " + message;
                    co_await server->broadcast(broadcast_msg);
                }
            );
            
            co_await session->start();
        };
    }
};

} // namespace coro
} // namespace network
} // namespace im

#endif // CORO_TCP_SERVER_HPP