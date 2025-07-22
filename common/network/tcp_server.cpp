#include "../utils/log_manager.hpp"
#include "IOService_pool.hpp"
#include "tcp_server.hpp"
TCPServer::TCPServer(unsigned short port)
        : io_context_(IOServicePool::GetInstance().GetIOService())
        , acceptor_(io_context_, tcp::endpoint(tcp::v4(), port))
        , signal_set_(io_context_, SIGINT, SIGTERM) {
    signal_set_.async_wait([this](boost::system::error_code const&, int) {
        if (LogManager::IsLoggingEnabled("tcp_server")) {
            LogManager::GetLogger("tcp_server")->info("Stopping server...");
        }
        stop();

        // 开始接受连接
        start_accpet();
    });
}

TCPServer::~TCPServer() { stop(); }


void TCPServer::stop() {
    if (!stopped_.exchange(true)) {
        error_code ec;
        // 取消信号监听
        signal_set_.cancel(ec);
        if (ec) {
            if (LogManager::IsLoggingEnabled("tcp_server")) {
                LogManager::GetLogger("tcp_server")
                        ->error("Error canceling signal set: {}", ec.message());
            }
        }
        // 关闭接受器
        acceptor_.close(ec);
        if (ec) {
            if (LogManager::IsLoggingEnabled("tcp_server")) {
                LogManager::GetLogger("tcp_server")
                        ->error("Error closing acceptor: {}", ec.message());
            }
        }

        // 关闭所有会话连接
        for (auto& session : sessions_) {
            session->close();
        }
        sessions_.clear();
        if (LogManager::IsLoggingEnabled("tcp_server")) {
            LogManager::GetLogger("tcp_server")->info("Stopped TCP server");
        }
    }
}

void TCPServer::set_connection_handler(std::function<void(TCPSession::Ptr)> callback) {
    connection_handler_ = std::move(callback);
}

void TCPServer::start_accpet() {
    // 异步接受新连接
    acceptor_.async_accept(
            [this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
                handle_accept(ec, std::move(socket));
            });
}

void TCPServer::handle_accept(const boost::system::error_code& ec,
                              boost::asio::ip::tcp::socket socket) {
    if (ec) {
        // 忽略操作取消的错误（正常关闭时发生）
        if (ec != boost::asio::error::operation_aborted) {
            if (LogManager::IsLoggingEnabled("tcp_server")) {
                LogManager::GetLogger("tcp_server")
                        ->error("Error accepting connection: {}", ec.message());
            }
        }
        return;
    }

    try {
        // 创建一个新的会话
        auto session = std::make_shared<TCPSession>(std::move(socket));

        // 添加到会话集合
        sessions_.insert(session);

        // 设置会话关闭是的清理回调
        session->set_close_callback([this, session] { remove_session(session); });

        // 调用新连接回调
        if(connection_handler_){
            connection_handler_(session);
        }

        // 启动会话
        session->start();
    } catch (error_code ec) {
        if(LogManager::IsLoggingEnabled("tcp_server")){
            LogManager::GetLogger("tcp_server")->error("❌Accept session error:{}",ec.message());
        }
    }

    start_accpet();
}

void TCPServer::remove_session(TCPSession::Ptr session) {
    // 从会话集合中移除
    auto it = sessions_.find(session);
    if (it != sessions_.end()) {
        sessions_.erase(it);
       if(LogManager::IsLoggingEnabled("tcp_server")){
            LogManager::GetLogger("tcp_server")->info("Session Removed:{},({} active sessions) ",session->remote_endpoint(),sessions_.size() );
        }
    }
}
