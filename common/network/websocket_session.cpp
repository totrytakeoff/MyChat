#include "websocket_server.hpp"
#include "websocket_session.hpp"
#include "../utils/log_manager.hpp"

namespace im {
namespace network {

using im::utils::LogManager;

static constexpr size_t max_send_queue_size = 1024;
static constexpr auto websocket_handshake_timeout = std::chrono::seconds(15);
static constexpr auto websocket_idle_timeout = std::chrono::seconds(30);

WebSocketSession::WebSocketSession(tcp::socket socket, ssl::context& ssl_ctx, WebSocketServer* server,
                                   MessageHandler messageHandler,
                                   ErrorHandler errorHandler)
        : ws_stream_(std::move(socket), ssl_ctx)
        , server_(server)
        , message_handler_(messageHandler)
        , error_handler_(errorHandler) {}


void WebSocketSession::start() {
    handshake_active_.store(true, std::memory_order_release);
    if (server_) {
        server_->record_handshake_started();
    }
    ssl_handshake_start_ = std::chrono::steady_clock::now();
    ws_stream_.next_layer().async_handshake(
            ssl::stream_base::server,
            [self = shared_from_this()](beast::error_code ec) {
                if (self->server_) {
                    self->server_->record_ssl_handshake(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() -
                                    self->ssl_handshake_start_));
                }
                if (ec) {
                    self->fail_and_close(ec, "WebSocket SSL handshake failed");
                    return;
                }
                self->on_ssl_handshake();
            }

    );
}



void WebSocketSession::close() {
    net::post(ws_stream_.get_executor(), [self = shared_from_this()]() {
        self->perform_close(true, {}, "WebSocket close requested");
    });
}

void WebSocketSession::send(const std::string& message) {
    net::post(ws_stream_.get_executor(),
              [self = shared_from_this(), msg = std::move(message)]() mutable {
                  if (self->closed_.load(std::memory_order_acquire)) {
                      return;
                  }
                  if (self->send_queue_.size() >= max_send_queue_size) {
                      self->fail_and_close({}, "Send queue overflow");
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
    upgrade_read_start_ = std::chrono::steady_clock::now();
    beast::http::async_read(ws_stream_.next_layer(), buffer_, *req,
        [self, req](beast::error_code ec, std::size_t /*bytes_transferred*/) {
            if (self->server_) {
                self->server_->record_upgrade_read(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() -
                                self->upgrade_read_start_));
            }
            if (ec) {
                self->fail_and_close(ec, "Read websocket upgrade request failed");
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

            websocket::stream_base::timeout timeout_opt;
            timeout_opt.handshake_timeout = websocket_handshake_timeout;
            timeout_opt.idle_timeout = websocket_idle_timeout;
            timeout_opt.keep_alive_pings = true;
            self->ws_stream_.set_option(timeout_opt);

            std::weak_ptr<WebSocketSession> weak_self = self;
            self->ws_stream_.control_callback([weak_self](websocket::frame_type kind,
                                                          beast::string_view payload) {
                auto session = weak_self.lock();
                if (!session) {
                    return;
                }
                if (!LogManager::IsLoggingEnabled("websocket_session")) {
                    return;
                }

                const char* kind_name = "unknown";
                switch (kind) {
                    case websocket::frame_type::close:
                        kind_name = "close";
                        break;
                    case websocket::frame_type::ping:
                        kind_name = "ping";
                        break;
                    case websocket::frame_type::pong:
                        kind_name = "pong";
                        break;
                    default:
                        break;
                }

                LogManager::GetLogger("websocket_session")
                        ->debug("WebSocket control frame: session={}, type={}, payload_size={}",
                                session->session_id_.empty() ? "<handshaking>" : session->session_id_,
                                kind_name,
                                payload.size());
            });

            // 使用解析到的请求完成WebSocket握手
            self->ws_accept_start_ = std::chrono::steady_clock::now();
            self->ws_stream_.async_accept(*req, [self](beast::error_code ec2) {
                if (self->server_) {
                    self->server_->record_ws_accept(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() -
                                    self->ws_accept_start_));
                }
                if (ec2) {
                    self->fail_and_close(ec2, "WebSocket handshake failed");
                    return;
                }
                // 清空HTTP读取时的缓冲
                self->buffer_.consume(self->buffer_.size());
                // 使用二进制帧进行通信（protobuf）
                self->ws_stream_.text(false);
                // 生成id并注册到服务器
                self->session_id_ = self->generate_id();
                self->registered_.store(true, std::memory_order_release);
                auto session_add_start = std::chrono::steady_clock::now();
                self->server_->add_session(self);
                self->server_->record_session_add(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - session_add_start));
                self->finish_handshake_tracking();
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
            self->fail_and_close(ec, "WebSocket read failed");
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
                    self->fail_and_close(ec, "WebSocket write failed");
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
    fail_and_close(ec, ec_msg);
}

void WebSocketSession::fail_and_close(beast::error_code ec, const std::string& ec_msg) {
    net::post(ws_stream_.get_executor(), [self = shared_from_this(), ec, ec_msg]() {
        self->perform_close(false, ec, ec_msg);
    });
}

void WebSocketSession::perform_close(bool graceful, beast::error_code ec,
                                     const std::string& ec_msg) {
    bool expected = false;
    if (!closed_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    finish_handshake_tracking();

    if (LogManager::IsLoggingEnabled("websocket_session")) {
        auto logger = LogManager::GetLogger("websocket_session");
        if (ec) {
            logger->warn("{}: {}", ec_msg, ec.message());
        } else {
            logger->info("{}", ec_msg);
        }
    }

    send_queue_.clear();
    buffer_.consume(buffer_.size());

    beast::error_code ignored_ec;
    if (graceful) {
        ws_stream_.close(websocket::close_code::normal, ignored_ec);
        if (ignored_ec && LogManager::IsLoggingEnabled("websocket_session")) {
            LogManager::GetLogger("websocket_session")
                    ->warn("WebSocket graceful close failed: {}", ignored_ec.message());
        }
    }

    auto& ssl_stream = ws_stream_.next_layer();
    ssl_stream.shutdown(ignored_ec);
    auto& socket = beast::get_lowest_layer(ws_stream_);
    socket.cancel(ignored_ec);
    socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
    socket.close(ignored_ec);

    if (registered_.exchange(false, std::memory_order_acq_rel) && server_) {
        server_->remove_session(shared_from_this());
    }

    if (close_callback_) {
        close_callback_(shared_from_this());
    }
}

void WebSocketSession::finish_handshake_tracking() {
    if (handshake_active_.exchange(false, std::memory_order_acq_rel) && server_) {
        server_->record_handshake_finished();
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
