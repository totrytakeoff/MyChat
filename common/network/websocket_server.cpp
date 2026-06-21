#include "websocket_server.hpp"
#include "websocket_session.hpp"

#include "../utils/log_manager.hpp"

#include <algorithm>
#include <vector>

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

WebSocketServerStats WebSocketServer::get_stats() const {
    WebSocketServerStats stats;
    stats.accept_ok = accept_ok_.load(std::memory_order_relaxed);
    stats.accept_fail = accept_fail_.load(std::memory_order_relaxed);
    stats.active_handshakes = active_handshakes_.load(std::memory_order_relaxed);
    stats.current_sessions = get_session_count();

    stats.ssl_handshake.count = ssl_handshake_count_.load(std::memory_order_relaxed);
    stats.ssl_handshake.total_ms = ssl_handshake_total_ms_.load(std::memory_order_relaxed);
    stats.ssl_handshake.max_ms = ssl_handshake_max_ms_.load(std::memory_order_relaxed);
    stats.upgrade_read.count = upgrade_read_count_.load(std::memory_order_relaxed);
    stats.upgrade_read.total_ms = upgrade_read_total_ms_.load(std::memory_order_relaxed);
    stats.upgrade_read.max_ms = upgrade_read_max_ms_.load(std::memory_order_relaxed);
    stats.ws_accept.count = ws_accept_count_.load(std::memory_order_relaxed);
    stats.ws_accept.total_ms = ws_accept_total_ms_.load(std::memory_order_relaxed);
    stats.ws_accept.max_ms = ws_accept_max_ms_.load(std::memory_order_relaxed);
    stats.session_add.count = session_add_count_.load(std::memory_order_relaxed);
    stats.session_add.total_ms = session_add_total_ms_.load(std::memory_order_relaxed);
    stats.session_add.max_ms = session_add_max_ms_.load(std::memory_order_relaxed);
    return stats;
}

void WebSocketServer::record_accept_ok() {
    accept_ok_.fetch_add(1, std::memory_order_relaxed);
}

void WebSocketServer::record_accept_fail() {
    accept_fail_.fetch_add(1, std::memory_order_relaxed);
}

void WebSocketServer::record_handshake_started() {
    active_handshakes_.fetch_add(1, std::memory_order_relaxed);
}

void WebSocketServer::record_handshake_finished() {
    uint64_t current = active_handshakes_.load(std::memory_order_relaxed);
    while (current > 0 &&
           !active_handshakes_.compare_exchange_weak(current, current - 1,
                                                     std::memory_order_relaxed,
                                                     std::memory_order_relaxed)) {
    }
}

void WebSocketServer::record_ssl_handshake(std::chrono::milliseconds duration) {
    record_duration(ssl_handshake_count_, ssl_handshake_total_ms_, ssl_handshake_max_ms_, duration);
}

void WebSocketServer::record_upgrade_read(std::chrono::milliseconds duration) {
    record_duration(upgrade_read_count_, upgrade_read_total_ms_, upgrade_read_max_ms_, duration);
}

void WebSocketServer::record_ws_accept(std::chrono::milliseconds duration) {
    record_duration(ws_accept_count_, ws_accept_total_ms_, ws_accept_max_ms_, duration);
}

void WebSocketServer::record_session_add(std::chrono::milliseconds duration) {
    record_duration(session_add_count_, session_add_total_ms_, session_add_max_ms_, duration);
}

void WebSocketServer::record_duration(std::atomic<uint64_t>& count,
                                      std::atomic<uint64_t>& total_ms,
                                      std::atomic<uint64_t>& max_ms,
                                      std::chrono::milliseconds duration) {
    const uint64_t value = static_cast<uint64_t>(std::max<int64_t>(0, duration.count()));
    count.fetch_add(1, std::memory_order_relaxed);
    total_ms.fetch_add(value, std::memory_order_relaxed);

    uint64_t current_max = max_ms.load(std::memory_order_relaxed);
    while (value > current_max &&
           !max_ms.compare_exchange_weak(current_max, value,
                                         std::memory_order_relaxed,
                                         std::memory_order_relaxed)) {
    }
}

void WebSocketServer::do_accept() {
    acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (ec) {
            record_accept_fail();
            if (LogManager::IsLoggingEnabled("websocket_server")) {
                LogManager::GetLogger("websocket_server")->error("Accept failed: {}", ec.message());
            }
            return;
        }
        record_accept_ok();
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

        std::vector<SessionPtr> sessions_to_close;
        {
            std::lock_guard lock(sessions_mutex_);
            sessions_to_close.reserve(sessions_.size());
            for (auto& session : sessions_) {
                sessions_to_close.push_back(session.second);
            }
            sessions_.clear();
        }

        for (auto& session : sessions_to_close) {
            if (session) {
                session->close();
            }
        }
    });
}

void WebSocketServer::add_session(SessionPtr session) {
    {
        std::lock_guard lock(sessions_mutex_);
        sessions_.emplace(session->get_session_id(), session);
    }
    
    // 调用连接建立回调
    if (connect_handler_) {
        connect_handler_(session);
    }
    
    if (LogManager::IsLoggingEnabled("websocket_server")) {
        LogManager::GetLogger("websocket_server")
                ->info("Session {} added, current session count: {}", 
                       session->get_session_id(), get_session_count());
    }
}

void WebSocketServer::remove_session(SessionPtr session) {
    {
        std::lock_guard lock(sessions_mutex_);
        sessions_.erase(session->get_session_id());
    }
    
    // 调用连接断开回调
    if (disconnect_handler_) {
        disconnect_handler_(session);
    }
    
    if (LogManager::IsLoggingEnabled("websocket_server")) {
        LogManager::GetLogger("websocket_server")
                ->info("Session {} removed, current session count: {}", 
                       session->get_session_id(), get_session_count());
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
