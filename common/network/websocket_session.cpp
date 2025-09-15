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
        self->server_->remove_session(self);

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
    // 手动读取HTTP升级请求以便在握手前提取token，然后使用该请求完成WS握手
    auto self = shared_from_this();
    auto req = std::make_shared<beast::http::request<beast::http::string_body>>();
    beast::http::async_read(ws_stream_.next_layer(), buffer_, *req,
        [self, req](beast::error_code ec, std::size_t /*bytes_transferred*/) {
            if (ec) {
                self->defaultErrorHandler(self, ec, "Read websocket upgrade request failed");
                return;
            }

            // 从URL查询参数中提取token
            std::string target = std::string(req->target());
            size_t token_pos = target.find("token=");
            if (token_pos != std::string::npos) {
                size_t start = token_pos + 6; // "token="长度
                size_t end = target.find('&', start);
                if (end == std::string::npos) {
                    end = target.length();
                }
                self->token_ = target.substr(start, end - start);
            }
            // 也可以从Authorization头部获取Token
            auto auth_it = req->find(beast::http::field::authorization);
            if (auth_it != req->end()) {
                std::string auth_value = std::string(auth_it->value());
                if (auth_value.rfind("Bearer ", 0) == 0) {
                    self->token_ = auth_value.substr(7);
                }
            }

            if (LogManager::IsLoggingEnabled("websocket_session")) {
                LogManager::GetLogger("websocket_session")
                        ->debug("Extracted token from handshake: {}",
                                self->token_.empty() ? "none" : "present");
            }

            // 使用解析到的请求完成WebSocket握手
            self->ws_stream_.async_accept(*req, [self](beast::error_code ec2) {
                if (ec2) {
                    self->defaultErrorHandler(self, ec2, "WebSocket handshake failed");
                    return;
                }
                // 清空HTTP读取时的缓冲
                self->buffer_.consume(self->buffer_.size());
                // 使用二进制帧进行通信（protobuf）
                self->ws_stream_.text(false);
                // 生成id并注册到服务器
                self->session_id_ = self->generate_id();
                self->server_->add_session(self);
                if (LogManager::IsLoggingEnabled("websocket_session")) {
                    LogManager::GetLogger("websocket_session")
                            ->info("Session {} successfully added to server, token: {}",
                                   self->session_id_, self->token_.empty() ? "none" : "present");
                }
                self->do_read();
            });
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

std::string WebSocketSession::get_client_ip() const {
    try {
        auto remote_endpoint = ws_stream_.next_layer().next_layer().remote_endpoint();
        return remote_endpoint.address().to_string();
    } catch (const std::exception& e) {
        if (LogManager::IsLoggingEnabled("websocket_session")) {
            LogManager::GetLogger("websocket_session")
                ->error("Failed to get client IP: {}", e.what());
        }
        return "unknown";
    }
}

} // namespace network
} // namespace im
