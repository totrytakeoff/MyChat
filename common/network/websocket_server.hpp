#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

/******************************************************************************
 *
 * @file       websocket_server.hpp
 * @brief      websocket 连接管理类
 *
 * @author     myself
 * @date       2025/07/30
 *
 *****************************************************************************/

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "../utils/thread_pool.hpp"


namespace im {
namespace network {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using ssl_stream = boost::asio::ssl::stream<tcp::socket>;
namespace ssl = boost::asio::ssl;  // 添加ssl命名空间

// 前置声明
class WebSocketSession;
class WebSocketServer;

// 类型别名
using SessionPtr = std::shared_ptr<WebSocketSession>;
using MessageHandler = std::function<void(SessionPtr, beast::flat_buffer&&)>;
using ErrorHandler = std::function<void(SessionPtr, beast::error_code)>;
using ConnectHandler = std::function<void(SessionPtr)>;
using DisconnectHandler = std::function<void(SessionPtr)>;

struct WebSocketDurationStats {
    uint64_t count{0};
    uint64_t total_ms{0};
    uint64_t max_ms{0};
};

struct WebSocketServerStats {
    uint64_t accept_ok{0};
    uint64_t accept_fail{0};
    uint64_t active_handshakes{0};
    uint64_t current_sessions{0};
    WebSocketDurationStats ssl_handshake;
    WebSocketDurationStats upgrade_read;
    WebSocketDurationStats ws_accept;
    WebSocketDurationStats session_add;
};

class WebSocketServer {
public:
    WebSocketServer(net::io_context& ioc, ssl::context& ssl_ctx, unsigned short port,
                    MessageHandler msg_handler);


    void broadcast(const std::string& message);

    void start();

    void stop();

    void add_session(SessionPtr session);

    void remove_session(SessionPtr session);
    void remove_session(const std::string &session_id);

    size_t get_session_count() const;

    WebSocketServerStats get_stats() const;

    void record_accept_ok();
    void record_accept_fail();
    void record_handshake_started();
    void record_handshake_finished();
    void record_ssl_handshake(std::chrono::milliseconds duration);
    void record_upgrade_read(std::chrono::milliseconds duration);
    void record_ws_accept(std::chrono::milliseconds duration);
    void record_session_add(std::chrono::milliseconds duration);

    SessionPtr get_session(const std::string& session_id) const{
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // 设置连接建立回调
    void set_connect_handler(ConnectHandler handler) {
        connect_handler_ = std::move(handler);
    }

    // 设置连接断开回调
    void set_disconnect_handler(DisconnectHandler handler) {
        disconnect_handler_ = std::move(handler);
    }

private:
    void do_accept();
    static void record_duration(std::atomic<uint64_t>& count,
                                std::atomic<uint64_t>& total_ms,
                                std::atomic<uint64_t>& max_ms,
                                std::chrono::milliseconds duration);

private:
    tcp::acceptor acceptor_;
    ssl::context& ssl_ctx_;
    std::unordered_map<std::string, SessionPtr> sessions_;
    mutable std::mutex sessions_mutex_;
    MessageHandler message_handler_;
    ConnectHandler connect_handler_;
    DisconnectHandler disconnect_handler_;

    std::atomic<uint64_t> accept_ok_{0};
    std::atomic<uint64_t> accept_fail_{0};
    std::atomic<uint64_t> active_handshakes_{0};
    std::atomic<uint64_t> ssl_handshake_count_{0};
    std::atomic<uint64_t> ssl_handshake_total_ms_{0};
    std::atomic<uint64_t> ssl_handshake_max_ms_{0};
    std::atomic<uint64_t> upgrade_read_count_{0};
    std::atomic<uint64_t> upgrade_read_total_ms_{0};
    std::atomic<uint64_t> upgrade_read_max_ms_{0};
    std::atomic<uint64_t> ws_accept_count_{0};
    std::atomic<uint64_t> ws_accept_total_ms_{0};
    std::atomic<uint64_t> ws_accept_max_ms_{0};
    std::atomic<uint64_t> session_add_count_{0};
    std::atomic<uint64_t> session_add_total_ms_{0};
    std::atomic<uint64_t> session_add_max_ms_{0};
};

} // namespace network
} // namespace im

#endif  // WEBSOCKET_SERVER_HPP
