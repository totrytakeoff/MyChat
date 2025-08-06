#include "websocket_server.hpp"
#include "websocket_session.hpp"

namespace im {
namespace network {

using im::utils::LogManager;

WebSocketServer::WebSocketServer(net::io_context& ioc, ssl::context& ssl_ctx, unsigned short port,
                                 MessageHandler msg_handler)
        : acceptor_(ioc, tcp::endpoint(tcp::v4(), port))
        , ssl_ctx_(ssl_ctx)
        , message_handler_(msg_handler) {}

size_t WebSocketServer::get_session_count() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

void WebSocketServer::do_accept() {
    acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (ec) {
            if (LogManager::IsLoggingEnabled("websocket_server")) {
                LogManager::GetLogger("websocket_server")->error("Accept failed: {}", ec.message());
            }
            return;
        }
        auto session = std::make_shared<WebSocketSession>(
                std::move(socket), ssl_ctx_, this, message_handler_);
        session->start();
        if (LogManager::IsLoggingEnabled("websocket_server")) {
            LogManager::GetLogger("websocket_server")
                    ->info("New WebSocket session created");
        }
        // 继续接受下一个连接
        do_accept();
                
    });
}


void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (const auto& [id, session] : sessions_) {
        session->send(message);
    }
}

void WebSocketServer::start() {
    if (LogManager::IsLoggingEnabled("websocket_server")) {
        LogManager::GetLogger("websocket_server")
                ->info("WebSocket server started on port {}", acceptor_.local_endpoint().port());
    }
    do_accept();
}

void WebSocketServer::stop() {
    net::post(acceptor_.get_executor(), [this] {
        acceptor_.close();

        std::lock_guard lock(sessions_mutex_);
        for (auto& session : sessions_) {
            session.second->close();
        }
        sessions_.clear();
    });
}

void WebSocketServer::add_session(SessionPtr session) {
    std::lock_guard lock(sessions_mutex_);
    sessions_.emplace(session->get_session_id(), session);
}

void WebSocketServer::remove_session(SessionPtr session) {
    std::lock_guard lock(sessions_mutex_);
    sessions_.erase(session->get_session_id());
    if (LogManager::IsLoggingEnabled("websocket_server")) {
        LogManager::GetLogger("websocket_server")
                ->info("Session {} removed, current session count: {}", session->get_session_id(),
                       sessions_.size());
    }
}

void WebSocketServer::remove_session(const std::string& session_id) {
    std::lock_guard lock(sessions_mutex_);
    sessions_.erase(session_id);
    if (LogManager::IsLoggingEnabled("websocket_server")) {
        LogManager::GetLogger("websocket_server")
                ->info("Session {} removed, current session count: {}", session_id,
                       sessions_.size());
    }
}

} // namespace network
} // namespace im

