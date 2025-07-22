#include <iostream>
#include "../utils/log_manager.hpp"
#include "IOService_pool.hpp"
#include "tcp_session.hpp"
TCPSession::TCPSession(tcp::socket socket)
        : socket_(std::move(socket))
        , heartbeat_timer_(socket_.get_executor())
        , read_timeout_timer_(socket_.get_executor())
        , body_buffer_(max_body_length) {
    // 初始化定时器为永不超时状态
    heartbeat_timer_.expires_at(std::chrono::steady_clock::time_point::max());
    read_timeout_timer_.expires_at(std::chrono::steady_clock::time_point::max());
}




void TCPSession::start() {
    try {
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->info("🚀Session started with remote endpoint: {}",
                           socket_.remote_endpoint().address().to_string());
        }

        socket_.set_option(tcp::no_delay(true));  // 禁用Nagle算法,减少延迟

        start_heartbeat();  // 启动心跳检测

        do_read_header();  // 开始读取消息头

    } catch (const std::exception& e) {
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")->error("Error starting session: {}", e.what());
        }
        close();
    }
}


void TCPSession::close() {
    if (socket_.is_open()) {
        error_code ec;

        // 优雅地关闭套接字
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);

        if (ec && LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")->error("Error closing socket: {}", ec.message());
        }

        // 停止定时器
        heartbeat_timer_.cancel();
        read_timeout_timer_.cancel();

        if (close_callback_) {
            close_callback_();
        }
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->info("🛑Session closed with remote endpoint: {}",
                           socket_.remote_endpoint().address().to_string());
        }
    }
}

tcp::endpoint TCPSession::remote_endpoint() const { return socket_.remote_endpoint(); }

void TCPSession::send(const std::string& message) {
    // 将发送操作投递到套接字的执行器中，确保线程安全
    net::post(socket_.get_executor(),[this,self=shared_from_this(), msg = std::move(message)]()mutable{
        bool write_in_progress = !send_quene_.empty();
        send_quene_.push_back(std::move(msg));
        
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

void TCPSession::start_heartbeat() {
    // 设置心跳定时器
    heartbeat_timer_.expires_after(heartbeat_interval);

    // 异步等待心跳定时器
    heartbeat_timer_.async_wait([this, self = shared_from_this()](const error_code& ec) {
        if (ec) return;  // 定时器被取消

        if (!socket_.is_open()) return;

        // 发送心跳消息
        send("HEARTBEAT");

        // 设置下一次心跳
        start_heartbeat();
    });
}

void TCPSession::reset_read_timeout() {
    // 设置读超时定时器
    read_timeout_timer_.expires_after(read_timeout);
    read_timeout_timer_.async_wait([this, self = shared_from_this()](const error_code& ec) {
        if (ec) return;  // 定时器被取消

        if (!socket_.is_open()) return;

        // 超时，关闭连接
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")
                    ->warn("Read timeout, closing session with remote endpoint: {}",
                           socket_.remote_endpoint().address().to_string());
        }
        close();
    });
}


void TCPSession::do_read_header() {
    // 重置读超时定时器
    reset_read_timeout();

    auto self(shared_from_this());

    // 异步读取消息头
    net::async_read(socket_,
                    net::buffer(header_),
                    [this, self](error_code ec, std::size_t bytes_transferred) {
                        // 取消读超时定时器
                        read_timeout_timer_.cancel();

                        if (ec) {
                            handle_error(ec);
                            return;
                        }

                        // 字节序转换,将网络字节序转换为本机字节序
                        size_t body_length = ntohl(*reinterpret_cast<size_t*>(header_.data()));

                        // 检查消息长度是否合法
                        if (body_length > max_body_length) {
                            if (LogManager::IsLoggingEnabled("tcp_session")) {
                                LogManager::GetLogger("tcp_session")
                                        ->error("Message too large:{}bytes from {}",
                                                body_length,
                                                remote_endpoint());
                            }
                            close();
                            return;
                        }

                        // 继续读取消息体
                        do_read_body(body_length);
                    });
}


void TCPSession::do_read_body(size_t length) {
    auto self(shared_from_this());

    body_buffer_.resize(length);

    net::async_read(socket_,
                    net::buffer(body_buffer_),
                    [this, self, length](error_code ec, size_t bytes_transferred) {
                        if (ec) {
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
    const std::string& message = send_quene_.front();

    // 构造带长度前缀的消息
    size_t length = htonl(static_cast<size_t>(message.size()));
    std::vector<net::const_buffer> buffers;
    buffers.push_back(net::buffer(&length, sizeof(length)));
    buffers.push_back(net::buffer(message));

    // 异步发送消息
    net::async_write(socket_, buffers, [this, self](error_code ec, std::size_t bytes_transferred) {
        if (ec) {
            handle_error(ec);
            return;
        }

        // 消息发送成功，从队列中删除
        send_quene_.pop_front();

        if (!send_quene_.empty()) {
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
    }
    else if(ec){
        if (LogManager::IsLoggingEnabled("tcp_session")) {
            LogManager::GetLogger("tcp_session")->error("❗TCP Connection Error: {}", ec.message());
        }
    }
}