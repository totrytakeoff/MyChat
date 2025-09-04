#include "coro_tcp_session.hpp"
#include "../common/utils/log_manager.hpp"
#include "../common/utils/global.hpp"

namespace im {
namespace network {
namespace coro {

using im::utils::LogManager;

CoroTCPSession::CoroTCPSession(tcp::socket socket)
    : socket_(std::move(socket))
    , read_timeout_timer_(socket_.get_executor())
    , send_channel_(socket_.get_executor(), max_send_queue_size)
    , body_buffer_(max_body_length) {
    
    if (socket_.is_open()) {
        try {
            remote_endpoint_ = socket_.remote_endpoint();
            
            // 启用TCP Keepalive
            socket_.set_option(net::socket_base::keep_alive(true));
            socket_.set_option(tcp::no_delay(true));
            
            if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
                LogManager::GetLogger("coro_tcp_session")
                    ->info("CoroTCPSession created for endpoint: {}", 
                           remote_endpoint_.address().to_string());
            }
        } catch (const std::exception& e) {
            if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
                LogManager::GetLogger("coro_tcp_session")
                    ->warn("Failed to get remote endpoint: {}", e.what());
            }
        }
    }
}

awaitable<void> CoroTCPSession::start() {
    try {
        if (!socket_.is_open()) {
            if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
                LogManager::GetLogger("coro_tcp_session")
                    ->warn("Cannot start session with closed socket");
            }
            co_return;
        }

        if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
            LogManager::GetLogger("coro_tcp_session")
                ->info("🚀Starting coroutine session with: {}", 
                       remote_endpoint_.address().to_string());
        }

        // 并发启动三个协程：读取、写入、心跳
        co_await (
            read_loop() ||
            write_loop() ||
            heartbeat_loop()
        );
        
    } catch (const std::exception& e) {
        if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
            LogManager::GetLogger("coro_tcp_session")
                ->error("Session start error: {}", e.what());
        }
    }
}

awaitable<void> CoroTCPSession::close() {
    if (is_closing_.exchange(true)) {
        co_return; // 已经在关闭中
    }

    if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
        LogManager::GetLogger("coro_tcp_session")
            ->info("🛑Closing coroutine session: {}", 
                   remote_endpoint_.address().to_string());
    }

    boost::system::error_code ec;
    
    // 关闭发送通道
    send_channel_.close();
    
    // 取消定时器
    read_timeout_timer_.cancel(ec);
    
    // 优雅关闭socket
    if (socket_.is_open()) {
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
    
    // 调用关闭回调
    if (close_handler_) {
        close_handler_();
    }
}

awaitable<void> CoroTCPSession::send(std::string message) {
    if (is_closing_ || !socket_.is_open()) {
        co_return;
    }
    
    try {
        // 使用协程通道发送消息，避免竞争条件
        co_await send_channel_.async_send(
            boost::system::error_code{}, 
            std::move(message), 
            net::use_awaitable
        );
    } catch (const std::exception& e) {
        if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
            LogManager::GetLogger("coro_tcp_session")
                ->warn("Send failed: {}", e.what());
        }
    }
}

awaitable<void> CoroTCPSession::read_loop() {
    try {
        while (!is_closing_ && socket_.is_open()) {
            // 重置读取超时
            read_timeout_timer_.expires_after(read_timeout);
            auto timeout_op = read_timeout_timer_.async_wait(net::use_awaitable);
            
            try {
                // 读取消息头（带超时）
                auto result = co_await (read_header() || std::move(timeout_op));
                
                if (result.index() == 1) {
                    // 超时
                    if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
                        LogManager::GetLogger("coro_tcp_session")
                            ->warn("Read timeout, closing session: {}", 
                                   remote_endpoint_.address().to_string());
                    }
                    break;
                }
            } catch (const std::exception& e) {
                handle_error("read_loop", boost::system::error_code{});
                break;
            }
        }
    } catch (const std::exception& e) {
        handle_error("read_loop", boost::system::error_code{});
    }
    
    co_await close();
}

awaitable<void> CoroTCPSession::write_loop() {
    try {
        while (!is_closing_ && socket_.is_open()) {
            try {
                // 等待发送通道中的消息
                auto [ec, message] = co_await send_channel_.async_receive(net::use_awaitable);
                
                if (ec || is_closing_) {
                    break;
                }
                
                // 构造消息格式：[4字节长度][1字节类型][消息体]
                uint32_t length = htonl(static_cast<uint32_t>(message.size()));
                uint8_t msg_type = static_cast<uint8_t>(HeaderMsgType::NORMAL);
                
                std::vector<net::const_buffer> buffers;
                buffers.push_back(net::buffer(&length, sizeof(length)));
                buffers.push_back(net::buffer(&msg_type, sizeof(msg_type)));
                buffers.push_back(net::buffer(message));
                
                // 异步写入
                co_await net::async_write(socket_, buffers, net::use_awaitable);
                
                if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
                    LogManager::GetLogger("coro_tcp_session")
                        ->debug("Message sent, size: {}", message.size());
                }
                
            } catch (const std::exception& e) {
                handle_error("write_loop", boost::system::error_code{});
                break;
            }
        }
    } catch (const std::exception& e) {
        handle_error("write_loop", boost::system::error_code{});
    }
}

awaitable<void> CoroTCPSession::heartbeat_loop() {
    try {
        net::steady_timer heartbeat_timer(socket_.get_executor());
        
        while (!is_closing_ && socket_.is_open()) {
            // 等待心跳间隔
            heartbeat_timer.expires_after(heartbeat_interval);
            co_await heartbeat_timer.async_wait(net::use_awaitable);
            
            if (is_closing_ || !socket_.is_open()) {
                break;
            }
            
            // 发送心跳
            co_await send_heartbeat();
            
            if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
                LogManager::GetLogger("coro_tcp_session")
                    ->debug("Heartbeat sent to: {}", 
                           remote_endpoint_.address().to_string());
            }
        }
    } catch (const std::exception& e) {
        handle_error("heartbeat_loop", boost::system::error_code{});
    }
}

awaitable<void> CoroTCPSession::read_header() {
    // 读取消息头（5字节：4字节长度 + 1字节类型）
    co_await net::async_read(socket_, net::buffer(header_), net::use_awaitable);
    
    // 解析长度和类型
    uint32_t body_length;
    std::memcpy(&body_length, header_.data(), sizeof(body_length));
    body_length = ntohl(body_length);
    
    uint8_t msg_type = header_[4];
    
    // 检查消息长度
    if (body_length > max_body_length) {
        throw std::runtime_error("Message too large: " + std::to_string(body_length));
    }
    
    // 根据消息类型处理
    switch (msg_type) {
        case static_cast<uint8_t>(HeaderMsgType::NORMAL):
            if (body_length > 0) {
                co_await read_body(body_length);
            }
            break;
            
        case static_cast<uint8_t>(HeaderMsgType::PING):
            // 回复PONG
            co_await send_heartbeat(); // 发送PONG
            break;
            
        case static_cast<uint8_t>(HeaderMsgType::PONG):
            // 收到PONG，重置超时
            read_timeout_timer_.cancel();
            break;
            
        default:
            if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
                LogManager::GetLogger("coro_tcp_session")
                    ->warn("Unknown message type: {}", msg_type);
            }
            throw std::runtime_error("Unknown message type");
    }
}

awaitable<void> CoroTCPSession::read_body(uint32_t length) {
    // 调整缓冲区大小
    if (length > body_buffer_.size()) {
        body_buffer_.resize(length);
    }
    
    // 读取消息体
    co_await net::async_read(socket_, 
        net::buffer(body_buffer_.data(), length), 
        net::use_awaitable);
    
    // 构造消息字符串
    std::string message(body_buffer_.data(), length);
    
    // 调用消息处理器
    if (message_handler_) {
        co_await message_handler_(message);
    } else {
        if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
            LogManager::GetLogger("coro_tcp_session")
                ->info("Received message (no handler): {}", message);
        }
    }
}

awaitable<void> CoroTCPSession::send_heartbeat() {
    uint32_t length = 0; // PING/PONG消息没有消息体
    uint8_t msg_type = static_cast<uint8_t>(HeaderMsgType::PING);
    
    std::vector<net::const_buffer> buffers;
    buffers.push_back(net::buffer(&length, sizeof(length)));
    buffers.push_back(net::buffer(&msg_type, sizeof(msg_type)));
    
    co_await net::async_write(socket_, buffers, net::use_awaitable);
}

void CoroTCPSession::handle_error(const std::string& operation, const boost::system::error_code& ec) {
    if (!is_closing_) {
        if (LogManager::IsLoggingEnabled("coro_tcp_session")) {
            LogManager::GetLogger("coro_tcp_session")
                ->error("Error in {}: {}", operation, ec.message());
        }
    }
}

} // namespace coro
} // namespace network
} // namespace im