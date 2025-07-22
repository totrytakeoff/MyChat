#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

/******************************************************************************
 *
 * @file       tcp_server.hpp
 * @brief      TCP Server类,用于管理TCP连接
 *
 * @author     myself
 * @date       2025/07/22
 *
 *****************************************************************************/

#include <atomic>
#include <boost/asio.hpp>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include "IOService_pool.hpp"
#include "tcp_session.hpp"

/**
 * @class TCPServer
 * @brief 基于IOServicePool的TCP服务器实现，管理所有TCP连接
 * 使用全局IOService池处理异步IO操作，实现高效的连接管理
 * 功能包括：
 * 1. 接受新的TCP连接
 * 2. 管理所有活动会话
 * 3. 多线程支持
 * 4. 优雅关闭机制
 */

class TCPServer {
public:
    /**
     * @brief 构造函数
     * @param port 监听端口
     * @param thread_count 工作线程数（默认为CPU核心数）
     */
    TCPServer(unsigned short port);

    /**
     * @brief 析构函数，自动停止服务器
     */
    ~TCPServer();


    /**
     * @brief 启动服务器
     * 开始监听并接受连接
     */
    void start();

    /**
     * @brief 停止服务器
     * 关闭所有连接并释放资源
     */
    void stop();

    /**
     * @brief 设置新连接建立时的回调函数
     * @param callback 回调函数，参数为TCPSession::Ptr
     */
    void set_connection_handler(std::function<void(TCPSession::Ptr)> callback);

private:
    // 开始接收新连接
    void start_accpet();

    // 处理新连接
    void handle_accept(const error_code& ec,tcp::socket socket);

    // 移除session
    void remove_session(TCPSession::Ptr session);

private:
    net::io_context& io_context_;          // I/O 上下文  使用引用,防止拷贝
    tcp::acceptor acceptor_;              // 连接接受器
    net::signal_set signal_set_;          // 信号处理器（用于捕获Ctrl+C）
    std::set<TCPSession::Ptr> sessions_;  // 活动会话集合
    std::atomic<bool> stopped_{false};    // 服务器停止标志
    std::function<void(TCPSession::Ptr)> connection_handler_;  // 新连接回调
};



#endif  // TCP_SERVER_HPP