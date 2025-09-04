#include "coro_tcp_server.hpp"
#include "../common/utils/log_manager.hpp"

namespace im {
namespace network {
namespace coro {

using im::utils::LogManager;

CoroTCPServer::CoroTCPServer(net::io_context& ioc, unsigned short port)
    : io_context_(ioc)
    , acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
    
    if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
        LogManager::GetLogger("coro_tcp_server")
            ->info("CoroTCPServer created on port: {}", port);
    }
}

CoroTCPServer::~CoroTCPServer() {
    if (is_running_) {
        // 注意：析构函数中不能使用协程，需要同步关闭
        boost::system::error_code ec;
        acceptor_.close(ec);
        
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : sessions_) {
            // 这里只能同步关闭socket，不能使用协程
            // 实际应用中应该在适当的地方调用stop()协程
            if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
                LogManager::GetLogger("coro_tcp_server")
                    ->info("Force closing session in destructor");
            }
        }
    }
}

awaitable<void> CoroTCPServer::start() {
    if (is_running_.exchange(true)) {
        co_return; // 已经在运行
    }
    
    if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
        LogManager::GetLogger("coro_tcp_server")
            ->info("🚀Starting coroutine TCP server...");
    }
    
    try {
        // 设置默认连接处理器（如果没有设置的话）
        if (!connection_handler_) {
            connection_handler_ = [this](SessionPtr session) -> awaitable<void> {
                co_await default_connection_handler(std::move(session));
            };
        }
        
        // 启动接受连接的协程
        co_await accept_loop();
        
    } catch (const std::exception& e) {
        if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
            LogManager::GetLogger("coro_tcp_server")
                ->error("Server start error: {}", e.what());
        }
        is_running_ = false;
        throw;
    }
}

awaitable<void> CoroTCPServer::stop() {
    if (!is_running_ || is_stopping_.exchange(true)) {
        co_return; // 未运行或已在停止中
    }
    
    if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
        LogManager::GetLogger("coro_tcp_server")
            ->info("🛑Stopping coroutine TCP server...");
    }
    
    // 关闭acceptor
    boost::system::error_code ec;
    acceptor_.close(ec);
    
    // 关闭所有会话
    std::vector<SessionPtr> sessions_to_close;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_to_close.reserve(sessions_.size());
        for (const auto& session : sessions_) {
            sessions_to_close.push_back(session);
        }
    }
    
    // 并发关闭所有会话
    std::vector<awaitable<void>> close_tasks;
    for (auto& session : sessions_to_close) {
        close_tasks.push_back(session->close());
    }
    
    // 等待所有会话关闭完成
    for (auto& task : close_tasks) {
        try {
            co_await std::move(task);
        } catch (const std::exception& e) {
            if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
                LogManager::GetLogger("coro_tcp_server")
                    ->warn("Error closing session: {}", e.what());
            }
        }
    }
    
    is_running_ = false;
    
    if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
        LogManager::GetLogger("coro_tcp_server")
            ->info("Coroutine TCP server stopped");
    }
}

awaitable<void> CoroTCPServer::broadcast(const std::string& message) {
    std::vector<SessionPtr> active_sessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        active_sessions.reserve(sessions_.size());
        for (const auto& session : sessions_) {
            active_sessions.push_back(session);
        }
    }
    
    if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
        LogManager::GetLogger("coro_tcp_server")
            ->debug("Broadcasting message to {} sessions", active_sessions.size());
    }
    
    // 并发发送到所有会话
    std::vector<awaitable<void>> send_tasks;
    for (auto& session : active_sessions) {
        send_tasks.push_back(session->send(message));
    }
    
    // 等待所有发送完成
    for (auto& task : send_tasks) {
        try {
            co_await std::move(task);
        } catch (const std::exception& e) {
            if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
                LogManager::GetLogger("coro_tcp_server")
                    ->warn("Error broadcasting to session: {}", e.what());
            }
        }
    }
}

awaitable<void> CoroTCPServer::accept_loop() {
    while (is_running_ && !is_stopping_) {
        try {
            // 异步接受新连接
            tcp::socket socket = co_await acceptor_.async_accept(net::use_awaitable);
            
            if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
                LogManager::GetLogger("coro_tcp_server")
                    ->info("✅New connection accepted from: {}", 
                           socket.remote_endpoint().address().to_string());
            }
            
            // 创建会话
            auto session = std::make_shared<CoroTCPSession>(std::move(socket));
            add_session(session);
            
            // 启动会话处理协程（不等待完成）
            net::co_spawn(io_context_, 
                handle_session(session), 
                net::detached);
                
        } catch (const std::exception& e) {
            if (!is_stopping_) {
                if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
                    LogManager::GetLogger("coro_tcp_server")
                        ->error("Accept error: {}", e.what());
                }
                
                // 短暂延迟后重试
                net::steady_timer timer(io_context_);
                timer.expires_after(std::chrono::milliseconds(100));
                co_await timer.async_wait(net::use_awaitable);
            }
        }
    }
}

awaitable<void> CoroTCPServer::handle_session(SessionPtr session) {
    try {
        // 设置会话关闭回调
        session->set_close_handler([this, session]() {
            remove_session(session);
        });
        
        // 调用用户定义的连接处理器
        if (connection_handler_) {
            co_await connection_handler_(session);
        }
        
    } catch (const std::exception& e) {
        if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
            LogManager::GetLogger("coro_tcp_server")
                ->error("Session handle error: {}", e.what());
        }
    }
    
    // 确保会话被移除
    remove_session(session);
}

awaitable<void> CoroTCPServer::default_connection_handler(SessionPtr session) {
    if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
        LogManager::GetLogger("coro_tcp_server")
            ->info("Using default connection handler for session");
    }
    
    // 设置默认消息处理器（简单echo）
    session->set_message_handler(
        [session](const std::string& message) -> awaitable<void> {
            if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
                LogManager::GetLogger("coro_tcp_server")
                    ->info("Default handler received: {}", message);
            }
            
            // Echo back the message
            std::string response = "Echo: " + message;
            co_await session->send(std::move(response));
        }
    );
    
    // 启动会话
    co_await session->start();
}

void CoroTCPServer::add_session(SessionPtr session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.insert(session);
    
    if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
        LogManager::GetLogger("coro_tcp_server")
            ->info("Session added, total sessions: {}", sessions_.size());
    }
}

void CoroTCPServer::remove_session(SessionPtr session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session);
    
    if (LogManager::IsLoggingEnabled("coro_tcp_server")) {
        LogManager::GetLogger("coro_tcp_server")
            ->info("Session removed, total sessions: {}", sessions_.size());
    }
}

} // namespace coro
} // namespace network
} // namespace im