#ifndef CORO_TCP_SESSION_HPP
#define CORO_TCP_SESSION_HPP

/******************************************************************************
 *
 * @file       coro_tcp_session.hpp
 * @brief      基于C++20协程的TCP Session类 - 示例实现
 *
 * @author     myself
 * @date       2025/01/21
 *
 *****************************************************************************/

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <coroutine>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace im {
namespace network {
namespace coro {

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace boost::asio::experimental::awaitable_operators;

// 协程相关类型定义
template<typename T>
using awaitable = net::awaitable<T>;
using executor_type = net::any_io_executor;

/**
 * @class CoroTCPSession
 * @brief 基于协程的TCP会话类
 * 
 * 优势：
 * 1. 消除回调地狱，代码线性化
 * 2. 异常处理更直观
 * 3. 状态管理更简单
 * 4. 业务逻辑更清晰
 */
class CoroTCPSession : public std::enable_shared_from_this<CoroTCPSession> {
public:
    using Ptr = std::shared_ptr<CoroTCPSession>;
    using MessageHandler = std::function<awaitable<void>(const std::string&)>;
    using CloseHandler = std::function<void()>;

    // 协程通道用于消息发送
    using SendChannel = net::experimental::channel<void(boost::system::error_code, std::string)>;

    explicit CoroTCPSession(tcp::socket socket);
    ~CoroTCPSession() = default;

    // 启动会话 - 协程版本
    awaitable<void> start();
    
    // 优雅关闭
    awaitable<void> close();
    
    // 发送消息 - 使用协程通道
    awaitable<void> send(std::string message);
    
    // 获取远程端点
    tcp::endpoint remote_endpoint() const { return remote_endpoint_; }
    
    // 设置回调函数
    void set_message_handler(MessageHandler handler) { message_handler_ = std::move(handler); }
    void set_close_handler(CloseHandler handler) { close_handler_ = std::move(handler); }

private:
    // 主要的协程函数
    awaitable<void> read_loop();           // 读取循环
    awaitable<void> write_loop();          // 写入循环  
    awaitable<void> heartbeat_loop();      // 心跳循环
    
    // 协程辅助函数
    awaitable<void> read_header();         // 读取消息头
    awaitable<void> read_body(uint32_t length); // 读取消息体
    awaitable<void> send_heartbeat();      // 发送心跳
    
    // 错误处理
    void handle_error(const std::string& operation, const boost::system::error_code& ec);

private:
    static constexpr std::chrono::seconds heartbeat_interval{30};
    static constexpr std::chrono::seconds read_timeout{120};
    static constexpr size_t max_body_length = 10 * 1024 * 1024;
    static constexpr size_t max_send_queue_size = 1024;

    tcp::socket socket_;
    tcp::endpoint remote_endpoint_;
    net::steady_timer read_timeout_timer_;
    
    // 协程通道替代发送队列
    SendChannel send_channel_;
    
    // 缓冲区
    std::array<char, 5> header_; // 4字节长度 + 1字节类型
    std::vector<char> body_buffer_;
    
    // 回调函数
    MessageHandler message_handler_;
    CloseHandler close_handler_;
    
    // 状态标志
    std::atomic<bool> is_closing_{false};
};

/**
 * @brief 协程工具类 - 提供常用的awaitable包装
 */
class CoroUtils {
public:
    // 延时等待
    static awaitable<void> delay(std::chrono::milliseconds ms) {
        net::steady_timer timer(co_await net::this_coro::executor);
        timer.expires_after(ms);
        co_await timer.async_wait(net::use_awaitable);
    }
    
    // 超时包装
    template<typename T>
    static awaitable<std::optional<T>> timeout(awaitable<T> op, std::chrono::milliseconds timeout_ms) {
        net::steady_timer timer(co_await net::this_coro::executor);
        timer.expires_after(timeout_ms);
        
        try {
            auto result = co_await (std::move(op) || timer.async_wait(net::use_awaitable));
            if (result.index() == 0) {
                co_return std::get<0>(result); // 操作完成
            } else {
                co_return std::nullopt; // 超时
            }
        } catch (...) {
            co_return std::nullopt;
        }
    }
};

} // namespace coro
} // namespace network
} // namespace im

#endif // CORO_TCP_SESSION_HPP