
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <functional>
#include <cstdint>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using error_code = boost::system::error_code;
using namespace std::chrono_literals;

class TCPSession : public std::enable_shared_from_this<TCPSession> {
public:
    using Ptr = std::shared_ptr<TCPSession>;
    using MessageHandler = std::function<void(const std::string&)>;
    using ErrorHandler = std::function<void(const error_code&)>;

    TCPSession(tcp::socket socket)
        : socket_(std::move(socket)),
          read_timer_(socket_.get_executor()),
          heartbeat_timer_(socket_.get_executor()) {
        read_timer_.expires_at(std::chrono::steady_clock::time_point::max());
    }

    void start(MessageHandler msg_handler, ErrorHandler err_handler) {
        msg_handler_ = std::move(msg_handler);
        err_handler_ = std::move(err_handler);
        
        // 设置TCP_NODELAY禁用Nagle算法
        socket_.set_option(tcp::no_delay(true));
        
        start_read();
        start_heartbeat();
    }

    void send(const std::string& message) {
        // 将消息加入发送队列
        net::post(socket_.get_executor(), 
            [this, self = shared_from_this(), msg = std::move(message)]() mutable {
                bool write_in_progress = !write_queue_.empty();
                write_queue_.push_back(std::move(msg));
                if (!write_in_progress) {
                    start_write();
                }
            });
    }

    void close() {
        if (socket_.is_open()) {
            error_code ec;
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            socket_.close(ec);
            read_timer_.cancel();
            heartbeat_timer_.cancel();
        }
    }

    tcp::endpoint remote_endpoint() const {
        return socket_.remote_endpoint();
    }

private:
    void start_read() {
        reset_read_timer();
        
        // 异步读取消息头（4字节长度）
        net::async_read(socket_, net::buffer(&msg_length_, sizeof(msg_length_)),
            [this, self = shared_from_this()](error_code ec, size_t /*bytes*/) {
                if (ec) {
                    handle_error(ec);
                    return;
                }
                
                // 将网络字节序转换为主机字节序
                msg_length_ = ntohl(msg_length_);
                
                // 检查长度是否有效
                if (msg_length_ > max_message_size) {
                    std::cerr << "Message too large: " << msg_length_ << " bytes\n";
                    close();
                    return;
                }
                
                // 准备缓冲区并读取消息体
                read_buffer_.resize(msg_length_);
                net::async_read(socket_, net::buffer(read_buffer_),
                    [this, self](error_code ec, size_t /*bytes*/) {
                        if (ec) {
                            handle_error(ec);
                            return;
                        }
                        
                        // 处理完整消息
                        if (msg_handler_) {
                            msg_handler_(std::string(read_buffer_.data(), read_buffer_.size()));
                        }
                        
                        // 继续读取下一条消息
                        start_read();
                    });
            });
    }

    void start_write() {
        if (write_queue_.empty()) return;
        
        const auto& msg = write_queue_.front();
        
        // 添加4字节长度前缀
        uint32_t length = htonl(static_cast<uint32_t>(msg.size()));
        std::vector<net::const_buffer> buffers;
        buffers.push_back(net::buffer(&length, sizeof(length)));
        buffers.push_back(net::buffer(msg));
        
        net::async_write(socket_, buffers,
            [this, self = shared_from_this()](error_code ec, size_t /*bytes*/) {
                if (ec) {
                    handle_error(ec);
                    return;
                }
                
                // 从队列中移除已发送消息
                write_queue_.pop_front();
                
                // 继续发送队列中的下一条消息
                if (!write_queue_.empty()) {
                    start_write();
                }
            });
    }

    void start_heartbeat() {
        heartbeat_timer_.expires_after(heartbeat_interval);
        heartbeat_timer_.async_wait(
            [this, self = shared_from_this()](error_code ec) {
                if (ec) return; // 定时器被取消
                
                if (!socket_.is_open()) return;
                
                // 发送心跳消息
                send("HEARTBEAT");
                start_heartbeat();
            });
    }

    void reset_read_timer() {
        read_timer_.expires_after(read_timeout);
        read_timer_.async_wait(
            [this, self = shared_from_this()](error_code ec) {
                if (!ec) {
                    // 读超时，关闭连接
                    std::cerr << "Connection timeout: " 
                              << remote_endpoint() << "\n";
                    close();
                }
            });
    }

    void handle_error(error_code ec) {
        if (ec == net::error::eof || 
            ec == net::error::connection_reset ||
            ec == net::error::operation_aborted) {
            // 正常关闭或连接重置
            close();
        } else if (ec) {
            std::cerr << "Connection error [" << remote_endpoint() 
                      << "]: " << ec.message() << "\n";
            if (err_handler_) err_handler_(ec);
            close();
        }
    }

    static constexpr size_t max_message_size = 10 * 1024 * 1024; // 10MB
    static constexpr auto read_timeout = 120s;    // 读操作超时时间
    static constexpr auto heartbeat_interval = 30s; // 心跳间隔

    tcp::socket socket_;
    net::steady_timer read_timer_;
    net::steady_timer heartbeat_timer_;
    
    uint32_t msg_length_ = 0;
    std::vector<char> read_buffer_;
    std::deque<std::string> write_queue_;
    
    MessageHandler msg_handler_;
    ErrorHandler err_handler_;
};

class TCPServer {
public:
    using SessionFactory = std::function<TCPSession::Ptr(tcp::socket)>;
    using ConnectionHandler = std::function<void(TCPSession::Ptr)>;

    TCPServer(net::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)),
          strand_(net::make_strand(io)) {
        start_accept();
    }

    void set_session_factory(SessionFactory factory) {
        session_factory_ = std::move(factory);
    }

    void set_connection_handler(ConnectionHandler handler) {
        connection_handler_ = std::move(handler);
    }

    void stop() {
        net::post(strand_, [this] {
            acceptor_.close();
            // 关闭所有活动连接
            for (auto& session : sessions_) {
                session->close();
            }
            sessions_.clear();
        });
    }

private:
    void start_accept() {
        acceptor_.async_accept(net::make_strand(acceptor_.get_executor()),
            [this](error_code ec, tcp::socket socket) {
                if (!acceptor_.is_open()) return;
                
                if (!ec) {
                    // 创建新会话
                    auto session = session_factory_ 
                        ? session_factory_(std::move(socket))
                        : std::make_shared<TCPSession>(std::move(socket));
                    
                    // 保存会话引用
                    sessions_.push_back(session);
                    
                    // 通知新连接
                    if (connection_handler_) {
                        connection_handler_(session);
                    }
                    
                    // 设置消息处理回调
                    session->start(
                        [session](const std::string& msg) {
                            // 默认消息处理：打印消息
                            std::cout << "Received from " 
                                      << session->remote_endpoint()
                                      << ": " << msg << "\n";
                        },
                        [this, session](const error_code& ec) {
                            // 错误处理：移除会话
                            remove_session(session);
                        }
                    );
                }
                
                start_accept();
            });
    }

    void remove_session(TCPSession::Ptr session) {
        auto it = std::find(sessions_.begin(), sessions_.end(), session);
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }
    }

    tcp::acceptor acceptor_;
    net::strand<net::io_context::executor_type> strand_;
    std::vector<TCPSession::Ptr> sessions_;
    
    SessionFactory session_factory_;
    ConnectionHandler connection_handler_;
};

// 使用示例
int main() {
    try {
        net::io_context io;
        TCPServer server(io, 12345);
        
        // 自定义会话创建逻辑
        server.set_session_factory([](tcp::socket socket) {
            return std::make_shared<TCPSession>(std::move(socket));
        });
        
        // 自定义连接处理逻辑
        server.set_connection_handler([](TCPSession::Ptr session) {
            std::cout << "New connection from: " 
                      << session->remote_endpoint() << "\n";
            
            // 发送欢迎消息
            session->send("Welcome to IM Server!");
        });
        
        // 启动IO线程池
        const int thread_count = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io] { io.run(); });
        }
        
        std::cout << "Server started on port 12345 with "
                  << thread_count << " threads\n";
        
        // 主线程等待退出信号
        std::string input;
        while (std::cin >> input, input != "exit");
        
        // 优雅关闭服务器
        server.stop();
        io.stop();
        
        // 等待线程结束
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
        
    } catch (std::exception& e) {
        std::cerr << "Server exception: " << e.what() << "\n";
    }
    
    return 0;
}