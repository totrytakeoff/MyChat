#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP

/******************************************************************************
 *
 * @file       websocket_session.hpp
 * @brief      websocket 会话管理类
 *
 * @author     myself
 * @date       2025/08/04
 *
 *****************************************************************************/

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "../utils/thread_pool.hpp"


namespace im {
namespace network {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = boost::asio::ssl;  // 添加ssl命名空间
using tcp = boost::asio::ip::tcp;
using ssl_stream = boost::asio::ssl::stream<tcp::socket>;

// 前置声明
class WebSocketSession;
class WebSocketServer;

// 类型别名
using SessionPtr = std::shared_ptr<WebSocketSession>;
using MessageHandler = std::function<void(SessionPtr, beast::flat_buffer&&)>;
using ErrorHandler = std::function<void(SessionPtr, beast::error_code)>;
using CloseHandler = std::function<void(SessionPtr)>;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket, ssl::context& ssl_ctx, WebSocketServer* server,
                              MessageHandler messageHandler = nullptr,
                              ErrorHandler errorHandler = nullptr);

    ~WebSocketSession() = default;

    void start();

    void close();

    void send(const std::string& message);


    const std::string& get_session_id() const { return session_id_; }

    const WebSocketServer* get_server() const { return server_; }

    void set_message_handler(MessageHandler &messageHandler) {
        message_handler_ = std::move(messageHandler);
    }

    void set_error_handler(ErrorHandler &errorHandler) {
        error_handler_ = std::move(errorHandler);
    }

    void set_close_handler(CloseHandler &closeHandler) {
        close_callback_ = std::move(closeHandler);
    }

private:
    void defaultMessageHandler(SessionPtr session, beast::flat_buffer&& buffer);

    void defaultErrorHandler(SessionPtr session, beast::error_code ec,std::string ec_msg="");

    void on_ssl_handshake();

    void do_read();

    void do_write();

    static std::string generate_id() {
        static std::atomic<size_t> counter{0};
        return "session_" + std::to_string(++counter);
    }

private:
    websocket::stream<ssl_stream> ws_stream_;
    beast::flat_buffer buffer_;
    std::deque<std::string> send_queue_;
    WebSocketServer* server_;
    MessageHandler message_handler_;
    ErrorHandler error_handler_;
    CloseHandler close_callback_;

    std::string session_id_;
};

} // namespace network
} // namespace im

#endif  // WEBSOCKET_SESSION_HPP