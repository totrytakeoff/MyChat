#ifndef TCP_SESSION_HPP
#define TCP_SESSION_HPP

/******************************************************************************
 *
 * @file       tcp_session.hpp
 * @brief      TCP Session类,用于处理单个TCP连接
 *
 * @author     myself
 * @date       2025/07/22
 *
 *****************************************************************************/

#include <boost/asio.hpp>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <vector>
#include "utils/global.hpp"

// 定义命名空间与简称
namespace net = boost::asio;
using tcp = net::ip::tcp;
using error_code = boost::system::error_code;



/**
 * @class TCPSession
 * @brief 表示一个TCP连接会话，处理该连接的所有通信
 *
 * 功能包括：
 * 1. 消息格式：4字节长度前缀 + 消息体（解决TCP粘包问题）
 * 2. 心跳机制：定期发送心跳包保持连接活跃
 * 3. 超时管理：读操作超时自动断开连接
 * 4. 线程安全的异步消息发送队列
 */

class TCPSession : public std::enable_shared_from_this<TCPSession> {
public:
    using Ptr = std::shared_ptr<TCPSession>;


    /**
     * @brief 构造函数
     * @param socket 已建立的TCP套接字
     */
    TCPSession(tcp::socket socket);

    ~TCPSession() = default;

    // 启动会话
    void start();

    /**
     * @brief 关闭连接
     */
    void close();

    /**
     * @brief 获取远程端点信息
     * @return 远程端点信息
     */
    tcp::endpoint remote_endpoint() const;

    /**
     * @brief 异步发送消息
     * @param message 要发送的消息内容
     */
    void send(const std::string& message);


    /**
     * @brief 设置连接关闭时的回调函数
     * @param callback 回调函数
     */
    void set_close_callback(std::function<void()> callback);

    /**
     * @brief 设置消息接收回调
     * @param callback 回调函数
     */
    void set_message_handler(std::function<void(const std::string&)> callback);

    // 发送ping
    void send_heartbeat(HeaderMsgType type = HeaderMsgType::PING);

    // 接收pong
    void handle_pong();

private:
    // 启动心跳检测
    void start_heartbeat();


    // 重置读定时器
    void reset_read_timeout();

    // 读取消息头（4字节长度）
    void do_read_header();

    // 读取消息体
    void do_read_body(uint32_t length);

    // 发送消息
    // void do_write(const std::string& message);
    void do_write();

    void handle_error(const error_code& ec);

private:
    static constexpr std::chrono::seconds heartbeat_interval{30};  // 心跳间隔
    static constexpr std::chrono::seconds read_timeout{120};       // 读超时时间
    static constexpr size_t max_body_length = 10 * 1024 * 1024;    // 最大消息体长度

    tcp::socket socket_;                                    // TCP套接字
    tcp::endpoint remote_endpoint_;                         // 远程端点信息
    net::steady_timer heartbeat_timer_;                     // 心跳定时器
    net::steady_timer read_timeout_timer_;                  // 读超时定时器

    std::array<char, HEADER_SIZE> header_;  // 消息头缓冲区
    std::vector<char> body_buffer_;         // 消息体缓冲区
    std::deque<std::string> send_queue_;    // 发送队列

    std::function<void(const std::string&)> message_handler_;  // 消息处理回调
    std::function<void()> close_callback_;                     // 连接关闭回调
};



#endif  // TCP_SESSION_HPP