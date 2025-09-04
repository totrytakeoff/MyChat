/******************************************************************************
 *
 * @file       performance_comparison.cpp
 * @brief      协程版本与回调版本的性能对比测试
 *
 * @author     myself
 * @date       2025/01/21
 *
 *****************************************************************************/

#include "coro_tcp_server.hpp"
#include "../common/network/tcp_server.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <atomic>
#include <vector>
#include <iostream>
#include <thread>
#include <future>

namespace im {
namespace network {
namespace benchmark {

using namespace std::chrono;
namespace net = boost::asio;

// ============================================================================
// 性能测试工具类
// ============================================================================

class PerformanceMetrics {
public:
    void start_timer() {
        start_time_ = high_resolution_clock::now();
    }
    
    void stop_timer() {
        end_time_ = high_resolution_clock::now();
    }
    
    double get_duration_ms() const {
        return duration_cast<microseconds>(end_time_ - start_time_).count() / 1000.0;
    }
    
    void add_connection() { connections_.fetch_add(1); }
    void add_message() { messages_.fetch_add(1); }
    void add_bytes(size_t bytes) { bytes_transferred_.fetch_add(bytes); }
    
    size_t get_connections() const { return connections_.load(); }
    size_t get_messages() const { return messages_.load(); }
    size_t get_bytes() const { return bytes_transferred_.load(); }
    
    double get_throughput_mbps() const {
        double duration_sec = get_duration_ms() / 1000.0;
        return (get_bytes() / (1024.0 * 1024.0)) / duration_sec;
    }
    
    double get_message_rate() const {
        double duration_sec = get_duration_ms() / 1000.0;
        return get_messages() / duration_sec;
    }

private:
    high_resolution_clock::time_point start_time_;
    high_resolution_clock::time_point end_time_;
    std::atomic<size_t> connections_{0};
    std::atomic<size_t> messages_{0};
    std::atomic<size_t> bytes_transferred_{0};
};

// ============================================================================
// 协程版本性能测试服务器
// ============================================================================

class CoroPerformanceServer {
public:
    explicit CoroPerformanceServer(net::io_context& ioc, unsigned short port, PerformanceMetrics& metrics)
        : server_(ioc, port), metrics_(metrics) {
        
        server_.set_connection_handler(
            [this](coro::CoroTCPServer::SessionPtr session) -> net::awaitable<void> {
                metrics_.add_connection();
                
                session->set_message_handler(
                    [this, session](const std::string& message) -> net::awaitable<void> {
                        metrics_.add_message();
                        metrics_.add_bytes(message.size());
                        
                        // Echo back the message
                        co_await session->send("Echo: " + message);
                        metrics_.add_bytes(message.size() + 6); // +6 for "Echo: "
                    }
                );
                
                co_await session->start();
            }
        );
    }
    
    net::awaitable<void> start() {
        co_await server_.start();
    }

private:
    coro::CoroTCPServer server_;
    PerformanceMetrics& metrics_;
};

// ============================================================================
// 回调版本性能测试服务器（简化版）
// ============================================================================

class CallbackPerformanceServer {
public:
    explicit CallbackPerformanceServer(net::io_context& ioc, unsigned short port, PerformanceMetrics& metrics)
        : acceptor_(ioc, net::ip::tcp::endpoint(net::ip::tcp::v4(), port))
        , metrics_(metrics) {}
    
    void start() {
        start_accept();
    }

private:
    void start_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, net::ip::tcp::socket socket) {
            if (!ec) {
                metrics_.add_connection();
                
                auto session = std::make_shared<CallbackSession>(std::move(socket), metrics_);
                session->start();
                
                start_accept();
            }
        });
    }
    
    class CallbackSession : public std::enable_shared_from_this<CallbackSession> {
    public:
        CallbackSession(net::ip::tcp::socket socket, PerformanceMetrics& metrics)
            : socket_(std::move(socket)), metrics_(metrics) {}
        
        void start() {
            do_read_header();
        }
        
    private:
        void do_read_header() {
            auto self = shared_from_this();
            net::async_read(socket_, net::buffer(header_),
                [this, self](boost::system::error_code ec, size_t) {
                    if (!ec) {
                        uint32_t length;
                        std::memcpy(&length, header_.data(), sizeof(length));
                        length = ntohl(length);
                        
                        if (length > 0 && length < 1024 * 1024) {
                            do_read_body(length);
                        } else {
                            do_read_header();
                        }
                    }
                });
        }
        
        void do_read_body(uint32_t length) {
            body_buffer_.resize(length);
            auto self = shared_from_this();
            
            net::async_read(socket_, net::buffer(body_buffer_),
                [this, self, length](boost::system::error_code ec, size_t) {
                    if (!ec) {
                        metrics_.add_message();
                        metrics_.add_bytes(length);
                        
                        std::string message(body_buffer_.data(), length);
                        std::string response = "Echo: " + message;
                        
                        do_write(response);
                        do_read_header();
                    }
                });
        }
        
        void do_write(const std::string& message) {
            uint32_t length = htonl(static_cast<uint32_t>(message.size()));
            uint8_t msg_type = 0;
            
            auto buffers = std::make_shared<std::vector<net::const_buffer>>();
            auto length_buf = std::make_shared<uint32_t>(length);
            auto type_buf = std::make_shared<uint8_t>(msg_type);
            auto msg_buf = std::make_shared<std::string>(message);
            
            buffers->push_back(net::buffer(length_buf.get(), sizeof(*length_buf)));
            buffers->push_back(net::buffer(type_buf.get(), sizeof(*type_buf)));
            buffers->push_back(net::buffer(*msg_buf));
            
            auto self = shared_from_this();
            net::async_write(socket_, *buffers,
                [this, self, length_buf, type_buf, msg_buf, buffers](boost::system::error_code ec, size_t bytes) {
                    if (!ec) {
                        metrics_.add_bytes(bytes);
                    }
                });
        }
        
        net::ip::tcp::socket socket_;
        PerformanceMetrics& metrics_;
        std::array<char, 5> header_;
        std::vector<char> body_buffer_;
    };
    
    net::ip::tcp::acceptor acceptor_;
    PerformanceMetrics& metrics_;
};

// ============================================================================
// 客户端负载生成器
// ============================================================================

class LoadGenerator {
public:
    static net::awaitable<void> generate_load(
        const std::string& host, 
        unsigned short port,
        size_t num_connections,
        size_t messages_per_connection,
        PerformanceMetrics& metrics) {
        
        std::vector<net::awaitable<void>> tasks;
        
        for (size_t i = 0; i < num_connections; ++i) {
            tasks.push_back(client_session(host, port, messages_per_connection, metrics));
        }
        
        // 并发执行所有客户端会话
        for (auto& task : tasks) {
            co_await std::move(task);
        }
    }

private:
    static net::awaitable<void> client_session(
        const std::string& host,
        unsigned short port,
        size_t message_count,
        PerformanceMetrics& metrics) {
        
        try {
            net::ip::tcp::socket socket(co_await net::this_coro::executor);
            net::ip::tcp::resolver resolver(co_await net::this_coro::executor);
            
            auto endpoints = co_await resolver.async_resolve(host, std::to_string(port), net::use_awaitable);
            co_await net::async_connect(socket, endpoints, net::use_awaitable);
            
            for (size_t i = 0; i < message_count; ++i) {
                std::string message = "Test message " + std::to_string(i);
                
                // 发送消息
                uint32_t length = htonl(static_cast<uint32_t>(message.size()));
                uint8_t msg_type = 0;
                
                std::vector<net::const_buffer> buffers;
                buffers.push_back(net::buffer(&length, sizeof(length)));
                buffers.push_back(net::buffer(&msg_type, sizeof(msg_type)));
                buffers.push_back(net::buffer(message));
                
                co_await net::async_write(socket, buffers, net::use_awaitable);
                
                // 接收响应
                std::array<char, 5> header;
                co_await net::async_read(socket, net::buffer(header), net::use_awaitable);
                
                uint32_t response_length;
                std::memcpy(&response_length, header.data(), sizeof(response_length));
                response_length = ntohl(response_length);
                
                if (response_length > 0) {
                    std::vector<char> response_buffer(response_length);
                    co_await net::async_read(socket, net::buffer(response_buffer), net::use_awaitable);
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Client error: " << e.what() << std::endl;
        }
    }
};

// ============================================================================
// 性能测试主函数
// ============================================================================

void run_performance_test() {
    const unsigned short CORO_PORT = 9001;
    const unsigned short CALLBACK_PORT = 9002;
    const size_t NUM_CONNECTIONS = 100;
    const size_t MESSAGES_PER_CONNECTION = 1000;
    const int TEST_DURATION_SECONDS = 30;
    
    std::cout << "=== 网络库性能对比测试 ===" << std::endl;
    std::cout << "连接数: " << NUM_CONNECTIONS << std::endl;
    std::cout << "每连接消息数: " << MESSAGES_PER_CONNECTION << std::endl;
    std::cout << "测试时长: " << TEST_DURATION_SECONDS << " 秒" << std::endl;
    std::cout << std::endl;
    
    // 测试协程版本
    {
        std::cout << "🔄 测试协程版本..." << std::endl;
        net::io_context ioc;
        PerformanceMetrics coro_metrics;
        
        CoroPerformanceServer server(ioc, CORO_PORT, coro_metrics);
        
        // 启动服务器
        net::co_spawn(ioc, server.start(), net::detached);
        
        // 等待服务器启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 启动负载测试
        coro_metrics.start_timer();
        
        net::co_spawn(ioc, 
            LoadGenerator::generate_load("127.0.0.1", CORO_PORT, NUM_CONNECTIONS, MESSAGES_PER_CONNECTION, coro_metrics),
            [&coro_metrics](std::exception_ptr) {
                coro_metrics.stop_timer();
            });
        
        // 运行指定时间
        auto start = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < TEST_DURATION_SECONDS) {
            ioc.run_for(std::chrono::milliseconds(100));
        }
        
        // 输出协程版本结果
        std::cout << "✅ 协程版本测试完成:" << std::endl;
        std::cout << "  连接数: " << coro_metrics.get_connections() << std::endl;
        std::cout << "  消息数: " << coro_metrics.get_messages() << std::endl;
        std::cout << "  数据量: " << coro_metrics.get_bytes() / 1024 << " KB" << std::endl;
        std::cout << "  吞吐量: " << coro_metrics.get_throughput_mbps() << " MB/s" << std::endl;
        std::cout << "  消息速率: " << coro_metrics.get_message_rate() << " msg/s" << std::endl;
        std::cout << "  测试时长: " << coro_metrics.get_duration_ms() << " ms" << std::endl;
        std::cout << std::endl;
    }
    
    // 测试回调版本
    {
        std::cout << "🔄 测试回调版本..." << std::endl;
        net::io_context ioc;
        PerformanceMetrics callback_metrics;
        
        CallbackPerformanceServer server(ioc, CALLBACK_PORT, callback_metrics);
        server.start();
        
        // 等待服务器启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 启动负载测试
        callback_metrics.start_timer();
        
        net::co_spawn(ioc,
            LoadGenerator::generate_load("127.0.0.1", CALLBACK_PORT, NUM_CONNECTIONS, MESSAGES_PER_CONNECTION, callback_metrics),
            [&callback_metrics](std::exception_ptr) {
                callback_metrics.stop_timer();
            });
        
        // 运行指定时间
        auto start = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < TEST_DURATION_SECONDS) {
            ioc.run_for(std::chrono::milliseconds(100));
        }
        
        // 输出回调版本结果
        std::cout << "✅ 回调版本测试完成:" << std::endl;
        std::cout << "  连接数: " << callback_metrics.get_connections() << std::endl;
        std::cout << "  消息数: " << callback_metrics.get_messages() << std::endl;
        std::cout << "  数据量: " << callback_metrics.get_bytes() / 1024 << " KB" << std::endl;
        std::cout << "  吞吐量: " << callback_metrics.get_throughput_mbps() << " MB/s" << std::endl;
        std::cout << "  消息速率: " << callback_metrics.get_message_rate() << " msg/s" << std::endl;
        std::cout << "  测试时长: " << callback_metrics.get_duration_ms() << " ms" << std::endl;
        std::cout << std::endl;
    }
    
    std::cout << "=== 测试完成 ===" << std::endl;
}

} // namespace benchmark
} // namespace network
} // namespace im

int main() {
    try {
        im::network::benchmark::run_performance_test();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}