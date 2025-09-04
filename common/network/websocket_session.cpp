#include "websocket_server.hpp"
#include "websocket_session.hpp"
#include "../utils/log_manager.hpp"

namespace im {
namespace network {

using im::utils::LogManager;

static constexpr size_t max_send_queue_size = 1024;

WebSocketSession::WebSocketSession(tcp::socket socket, ssl::context& ssl_ctx, WebSocketServer* server,
                                   MessageHandler messageHandler,
                                   ErrorHandler errorHandler)
        : ws_stream_(std::move(socket), ssl_ctx)
        , server_(server)
        , message_handler_(messageHandler)
        , error_handler_(errorHandler) {}


void WebSocketSession::start() {
    ws_stream_.next_layer().async_handshake(
            ssl::stream_base::server,
            [self = shared_from_this()](beast::error_code ec) {
                if (ec) {
                    self->defaultErrorHandler(self, ec, "WebSocket SSL handshake failed");
                    return;
                }
                self->on_ssl_handshake();
            }

    );
}



void WebSocketSession::close() {
    net::post(ws_stream_.get_executor(), [self = shared_from_this()]() {
        beast::error_code ec;
        self->ws_stream_.close(websocket::close_code::normal, ec);

        if (ec) {
            self->defaultErrorHandler(self, ec, "WebSocket close failed");
            return;
        }
        if (self->close_callback_) {
            self->close_callback_(self);
        }
    });
}

void WebSocketSession::send(const std::string& message) {
    net::post(ws_stream_.get_executor(),
              [self = shared_from_this(), msg = std::move(message)]() mutable {
                  if (self->send_queue_.size() >= max_send_queue_size) {
                      beast::error_code ec;
                      self->defaultErrorHandler(self, ec, "Send queue overflow");
                      return;
                  }
                  // 将消息添加到发送队列
                  self->send_queue_.emplace_back(std::move(msg));
                  if (self->send_queue_.size() == 1) {
                      self->do_write();
                  }
              });
}



void WebSocketSession::on_ssl_handshake() {
    // 异步websocket握手
    ws_stream_.async_accept([self = shared_from_this()](beast::error_code ec) {
        if (ec) {
            self->defaultErrorHandler(self, ec, "WebSocket handshake failed");
            return;
        }
        // 注意要先生成id再添加到服务器
        self->session_id_ = self->generate_id();
        self->server_->add_session(self);
        if (LogManager::IsLoggingEnabled("websocket_session")) {
            LogManager::GetLogger("websocket_session")
                    ->info("Session {} successfully added to server", self->session_id_);
        }
        self->do_read();
    });
}

void WebSocketSession::do_read() {
    ws_stream_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec,
                                                               std::size_t bytes_transferred) {
        if (ec) {
            self->defaultErrorHandler(self, ec, "WebSocket read failed");
            return;
        }
        if (self->message_handler_) {
            self->message_handler_(self, std::move(self->buffer_));
        } else {
            self->defaultMessageHandler(self, std::move(self->buffer_));
        }
        self->buffer_.consume(self->buffer_.size());  // 清空缓冲区
        // 继续读取下一个消息
        self->do_read();
    });
}


void WebSocketSession::do_write() {
    ws_stream_.async_write(
            net::buffer(send_queue_.front()),
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
                if (ec) {
                    self->defaultErrorHandler(self, ec, "WebSocket write failed");
                    return;
                }
                self->send_queue_.pop_front();
                if (!self->send_queue_.empty()) {
                    self->do_write();  // 继续发送下一个消息
                }
            });
}



void WebSocketSession::defaultMessageHandler(SessionPtr session, beast::flat_buffer&& buffer) {
    // 默认消息处理：记录收到的消息
    if (LogManager::IsLoggingEnabled("websocket_session")) {
        std::string message = beast::buffers_to_string(buffer.data());
        LogManager::GetLogger("websocket_session")
                ->info("Received message from session {}: {}", session_id_, message);
    }
}

void WebSocketSession::defaultErrorHandler(SessionPtr session, beast::error_code ec,
                                           std::string ec_msg) {
    if (ec == websocket::error::closed || ec == net::error::eof ||
        ec == net::error::connection_reset) {
        server_->remove_session(shared_from_this());

    } else {
        if (LogManager::IsLoggingEnabled("websocket_session")) {
            LogManager::GetLogger("websocket_session")->error("{}: {}", ec_msg, ec.message());
        }
    }
}

} // namespace network
} // namespace im
