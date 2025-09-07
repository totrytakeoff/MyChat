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

private:
    tcp::acceptor acceptor_;
    ssl::context& ssl_ctx_;
    std::unordered_map<std::string, SessionPtr> sessions_;
    mutable std::mutex sessions_mutex_;
    MessageHandler message_handler_;
    ConnectHandler connect_handler_;
    DisconnectHandler disconnect_handler_;
};

} // namespace network
} // namespace im

#endif  // WEBSOCKET_SERVER_HPP