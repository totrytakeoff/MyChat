#include <iostream>
#include "../utils/log_manager.hpp"
#include "IOService_pool.hpp"
#include "tcp_session.hpp"

namespace im {
namespace network {

using im::utils::LogManager;

static constexpr size_t max_send_queue_size = 1024;
TCPSession::TCPSession(tcp::socket socket)
        : socket_(std::move(socket))
        , heartbeat_timer_(socket_.get_executor())
        , read_timeout_timer_(socket_.get_executor())
        , body_buffer_(max_body_length) {
    // 问题：初始化定时器为max()可能导致异常行为
    // 解决：初始化为最小时间点，表示定时器未启动
    heartbeat_timer_.expires_at(std::chrono::steady_clock::time_point::min());
    read_timeout_timer_.expires_at(std::chrono::steady_clock::time_point::min());


    // 启用TCP Keepalive
    socket_.set_option(net::socket_base::keep_alive(true));

// 平台特定的参数设置
#if defined(__linux__)
    int fd = socket_.native_handle();
    int keepidle = 30;  // 空闲30秒后开始探测
    int keepintvl = 5;  // 每5秒探测一次
    int keepcnt = 3;    // 探测3次

    setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
#endif

    // 只有在socket已连接的情况下才获取remote_endpoint
    if (socket_.is_open()) {
        try {
            remote_endpoint_ = socket_.remote_endpoint();
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")
                        ->info("TCPSession created for endpoint: {}",
                               remote_endpoint_.address().to_string());
            }
        } catch (const std::exception& e) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")
                        ->warn("Failed to get remote endpoint: {}", e.what());
            }
        }
    }
}



void TCPSession::start() {
    try {
        if (socket_.is_open()) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")
                        ->info("🚀Session started with remote endpoint: {}",
                               remote_endpoint_.address().to_string());
            }

            socket_.set_option(tcp::no_delay(true));  // 禁用Nagle算法,减少延迟

            start_heartbeat();  // 启动心跳检测

            do_read_header();  // 开始读取消息头
        } else {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")->warn("Session started with closed socket");
            }
        }
    } catch (const std::exception& e) {
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")->error("Error starting session: {}", e.what());
        }
        close();
    }
}


void TCPSession::close() {
    if (LogManager::IsLoggingEnabled("tcp_session")) {
        LogManager::GetLogger("tcp_session")
                ->info("Closing session with remote endpoint: {}",
                       remote_endpoint_.address().to_string());
    }

    if (socket_.is_open()) {
        error_code ec;

        // 优雅地关闭套接字
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->info("Shutting down socket with remote endpoint: {}",
                           remote_endpoint_.address().to_string());
        }

        socket_.shutdown(tcp::socket::shutdown_both, ec);
        if (ec) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")
                        ->warn("Error shutting down socket: {}", ec.message());
            }
        }

        socket_.close(ec);
        if (ec) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")
                        ->warn("Error closing socket: {}", ec.message());
            }
        }

        // 停止定时器
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")->info("Canceling timers");
        }

        heartbeat_timer_.cancel();
        read_timeout_timer_.cancel();

        if (close_callback_) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")->info("Calling close callback");
            }
            close_callback_();
        }

        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->info("🛑Session closed with remote endpoint: {}",
                           remote_endpoint_.address().to_string());
        }
    } else {
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->info("Socket already closed for endpoint: {}",
                           remote_endpoint_.address().to_string());
        }
    }
}

tcp::endpoint TCPSession::remote_endpoint() const { return remote_endpoint_; }

void TCPSession::send(const std::string& message) {
    // 将发送操作投递到套接字的执行器中，确保线程安全
    net::post(socket_.get_executor(),
              [this, self = shared_from_this(), msg = std::move(message)]() mutable {
                  if (send_queue_.size() >= max_send_queue_size) {
                      if (LogManager::IsLoggingEnabled("tcp_session")) {
                          LogManager::GetLogger("tcp_session")
                                  ->warn("Message dropped, send queue full:{}",
                                         remote_endpoint().address().to_string());
                      }
                      return;
                  }

                  bool write_in_progress = !send_queue_.empty();
                  send_queue_.emplace_back(std::move(msg));

                  // 如果没有正在写入的数据，则开始写入数据
                  if (!write_in_progress) {
                      do_write();
                  }
              });
}

void TCPSession::set_close_callback(std::function<void()> callback) {
    close_callback_ = std::move(callback);
}

void TCPSession::set_message_handler(std::function<void(const std::string&)> callback) {
    message_handler_ = std::move(callback);
}

void TCPSession::send_heartbeat(HeaderMsgType type) {
    if (type != HeaderMsgType::PING && type != HeaderMsgType::PONG) {
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->warn("Invalid heartbeat type: {}, only PING and PONG are allowed",
                           static_cast<int>(type));
        }
        return;
    }

    uint32_t length = 0;
    // 问题：虽然变量名为uint8_t，但后续buffer操作使用sizeof(msg_type)仍为1字节
    // 这里没有实际问题，但为了代码清晰度，明确使用uint8_t类型
    uint8_t msg_type = static_cast<uint8_t>(type);

    std::vector<net::const_buffer> buffers;
    buffers.push_back(net::buffer(&length, sizeof(length)));
    buffers.push_back(net::buffer(&msg_type, sizeof(msg_type)));

    net::async_write(
            socket_,
            buffers,
            [this, self = shared_from_this()](error_code ec, std::size_t bytes_transferred) {
                if (ec) {
                    handle_error(ec);
                }
            });
}


void TCPSession::handle_pong() {
    reset_read_timeout();
    if (LogManager::IsLoggingEnabled("tcp_session")) {
        LogManager::GetLogger("tcp_session")
                ->info("handle_pong from {}", remote_endpoint_.address().to_string());
    }
}
void TCPSession::start_heartbeat() {
    // 设置心跳定时器
    heartbeat_timer_.expires_after(heartbeat_interval);

    // 异步等待心跳定时器
    heartbeat_timer_.async_wait([this, self = shared_from_this()](const error_code& ec) {
        if (ec) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")
                        ->info("Heartbeat timer cancelled: {}", ec.message());
            }
            return;  // 定时器被取消
        }

        if (!socket_.is_open()) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")->info("Socket closed, stopping heartbeat");
            }
            return;
        }

        // 发送心跳消息
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->info("Sending heartbeat to: {}", remote_endpoint().address().to_string());
        }

        // 发送ping消息
        send_heartbeat();

        // 设置下一次心跳
        start_heartbeat();
    });
}

void TCPSession::reset_read_timeout() {
    // 设置读超时定时器
    read_timeout_timer_.expires_after(read_timeout);
    read_timeout_timer_.async_wait([this, self = shared_from_this()](const error_code& ec) {
        if (ec) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")
                        ->info("Read timeout timer cancelled: {}", ec.message());
            }
            return;  // 定时器被取消
        }

        if (!socket_.is_open()) {
            if (LogManager::IsLoggingEnabled("tcp_session")) {
                LogManager::GetLogger("tcp_session")
                        ->info("Socket closed, stopping read timeout timer");
            }
            return;
        }

        // 超时，关闭连接
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->warn("Read timeout, closing session with remote endpoint: {}",
                           remote_endpoint_.address().to_string());
        }
        close();
    });
}


void TCPSession::do_read_header() {
    reset_read_timeout();  // 重置读超时定时器
    if (LogManager::IsLoggingEnabled("tcp_session")) {
        LogManager::GetLogger("tcp_session")
                ->info("Starting to read header from: {}", remote_endpoint_.address().to_string());
    }


    auto self(shared_from_this());

    // 异步读取消息头
    net::async_read(socket_,
                    net::buffer(header_),
                    [this, self](error_code ec, std::size_t bytes_transferred) {
                        // 取消读超时定时器
                        read_timeout_timer_.cancel();

                        if (ec) {
                            if (LogManager::IsLoggingEnabled("tcp_session")) {
                                LogManager::GetLogger("tcp_session")
                                        ->info("Read header error: {} from {}",
                                               ec.message(),
                                               remote_endpoint().address().to_string());
                            }
                            handle_error(ec);
                            return;
                        }

                        // 字节序转换,将网络字节序转换为本机字节序
                        // uint32_t body_length =
                        // ntohl(*reinterpret_cast<uint32_t*>(header_.data()));
                        // 使用memcpy安全解析长度
                        uint32_t body_length;
                        std::memcpy(&body_length, header_.data(), sizeof(body_length));
                        body_length = ntohl(body_length);

                        uint8_t msg_type = *reinterpret_cast<uint8_t*>(&header_[4]);

                        // 检查消息长度是否合法
                        if (body_length > max_body_length) {
                            if (LogManager::IsLoggingEnabled("tcp_session")) {
                                LogManager::GetLogger("tcp_session")
                                        ->error("Message too large:{}bytes from {}",
                                                body_length,
                                                remote_endpoint().address().to_string());
                            }
                            close();
                            return;
                        }


                        switch (msg_type) {
                            case static_cast<uint8_t>(HeaderMsgType::NORMAL):
                                // 处理消息
                                // 继续读取消息体
                                if (LogManager::IsLoggingEnabled("tcp_session")) {
                                    LogManager::GetLogger("tcp_session")
                                            ->info("Header read, body length: {} from {}",
                                                   body_length,
                                                   remote_endpoint_.address().to_string());
                                }
                                do_read_body(body_length);
                                return;
                                break;

                            case static_cast<uint8_t>(HeaderMsgType::PING):
                                send_heartbeat(HeaderMsgType::PONG);
                                break;
                            case static_cast<uint8_t>(HeaderMsgType::PONG):
                                // 处理PONG
                                handle_pong();
                                break;
                            default:
                                // 问题：缺少对未知消息类型的处理
                                // 解决：添加默认处理分支，记录日志并关闭连接
                                if (LogManager::IsLoggingEnabled("tcp_session")) {
                                    LogManager::GetLogger("tcp_session")
                                            ->warn("Unknown message type: {} from {}",
                                                   msg_type,
                                                   remote_endpoint().address().to_string());
                                }
                                close();
                                return;
                        }
                        do_read_header();
                    });
}


void TCPSession::do_read_body(uint32_t length) {
    reset_read_timeout();  // 重置读超时定时器

    auto self(shared_from_this());

    // 问题：每次都重新分配body_buffer_可能导致性能问题
    // 解决：复用缓冲区，仅在需要更大空间时才重新分配
    if (length > body_buffer_.size()) {
        body_buffer_.resize(length);
    }

    net::async_read(socket_,
                    net::buffer(body_buffer_.data(), length),
                    [this, self, length](error_code ec, size_t bytes_transferred) {
                        // 取消读超时定时器
                        read_timeout_timer_.cancel();

                        if (ec) {
                            if (LogManager::IsLoggingEnabled("tcp_session")) {
                                LogManager::GetLogger("tcp_session")
                                        ->info("Read body error: {} from {}",
                                               ec.message(),
                                               remote_endpoint().address().to_string());
                            }
                            handle_error(ec);
                            return;
                        }

                        // 构造消息
                        std::string message(body_buffer_.data(), length);

                        // 调用消息处理函数
                        if (message_handler_) {
                            message_handler_(message);
                        } else {
                            LogManager::GetLogger("tcp_session")
                                    ->warn("🟠Message handler not set,recved message:{}", message);
                        }
                        // 继续读取下一条消息
                        do_read_header();
                    });
}


void TCPSession::do_write() {
    auto self(shared_from_this());

    //  从队列中获取一条消息
    const std::string& message = send_queue_.front();

    // 构造带长度前缀的消息
    uint32_t length = htonl(static_cast<uint32_t>(message.size()));
    // 问题：原代码使用static_cast<uint32_t>转换HeaderMsgType::NORMAL，导致msg_type占用4字节
    // 解决：使用static_cast<uint8_t>确保msg_type字段为1字节
    uint8_t msg_type = static_cast<uint8_t>(HeaderMsgType::NORMAL);
    std::vector<net::const_buffer> buffers;
    buffers.push_back(net::buffer(&length, sizeof(length)));
    buffers.push_back(net::buffer(&msg_type, sizeof(msg_type)));
    buffers.push_back(net::buffer(message));

    // 异步发送消息
    net::async_write(socket_, buffers, [this, self](error_code ec, std::size_t bytes_transferred) {
        if (ec) {
            handle_error(ec);
            return;
        }

        // 消息发送成功，从队列中删除
        send_queue_.pop_front();

        if (!send_queue_.empty()) {
            // 队列中还有消息，继续发送
            do_write();
        }
    });
}


void TCPSession::handle_error(const error_code& ec) {
    // 处理正常关闭的错误
    if (ec == net::error::eof || ec == net::error::connection_reset ||
        ec == net::error::operation_aborted) {
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")->info("Connection closed: {}", ec.message());
        }
    } else if (ec) {
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")->error("❗TCP Connection Error: {}", ec.message());
        }
    }

    // 关闭会话
    close();
}

} // namespace network
} // namespace im
