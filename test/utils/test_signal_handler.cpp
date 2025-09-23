/**
 * @file test_signal_handler.cpp
 * @brief SignalHandler 类的单元测试
 * @details 测试信号处理器的各种功能，包括信号注册、回调执行、优雅关闭等
 */

#include <gtest/gtest.h>
#include "signal_handler.hpp"
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace im::utils;
using namespace std::chrono_literals;

class SignalHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试前重置状态
        auto& handler = SignalHandler::getInstance();
        handler.cleanup();
        handler.reset();
        
        // 重置测试变量
        callback_called_ = false;
        callback_signal_ = 0;
        callback_name_.clear();
        callback_count_ = 0;
    }
    
    void TearDown() override {
        // 每个测试后清理
        auto& handler = SignalHandler::getInstance();
        handler.cleanup();
        handler.reset();
    }
    
    // 测试用的回调函数
    void TestCallback(int signal, const std::string& signal_name) {
        callback_called_ = true;
        callback_signal_ = signal;
        callback_name_ = signal_name;
        callback_count_++;
    }
    
    // 测试变量
    std::atomic<bool> callback_called_{false};
    std::atomic<int> callback_signal_{0};
    std::string callback_name_;
    std::atomic<int> callback_count_{0};
};

// 测试单例模式
TEST_F(SignalHandlerTest, SingletonPattern) {
    auto& handler1 = SignalHandler::getInstance();
    auto& handler2 = SignalHandler::getInstance();
    
    // 应该是同一个实例
    EXPECT_EQ(&handler1, &handler2);
}

// 测试基本信号注册
TEST_F(SignalHandlerTest, BasicSignalRegistration) {
    auto& handler = SignalHandler::getInstance();
    
    // 注册信号处理器
    bool success = handler.registerSignalHandler(SIGUSR1, 
        [this](int sig, const std::string& name) {
            TestCallback(sig, name);
        }, "SIGUSR1");
    
    EXPECT_TRUE(success);
    
    // 检查已注册的信号
    auto signals = handler.getRegisteredSignals();
    EXPECT_TRUE(std::find(signals.begin(), signals.end(), SIGUSR1) != signals.end());
}

// 测试信号回调执行
TEST_F(SignalHandlerTest, SignalCallbackExecution) {
    auto& handler = SignalHandler::getInstance();
    
    // 注册信号处理器
    handler.registerSignalHandler(SIGUSR1, 
        [this](int sig, const std::string& name) {
            TestCallback(sig, name);
        }, "SIGUSR1");
    
    // 发送信号
    std::raise(SIGUSR1);
    
    // 等待信号处理完成
    std::this_thread::sleep_for(100ms);
    
    // 验证回调被调用
    EXPECT_TRUE(callback_called_.load());
    EXPECT_EQ(callback_signal_.load(), SIGUSR1);
    EXPECT_EQ(callback_name_, "SIGUSR1");
}

// 测试优雅关闭信号注册
TEST_F(SignalHandlerTest, GracefulShutdownRegistration) {
    auto& handler = SignalHandler::getInstance();
    
    bool success = handler.registerGracefulShutdown(
        [this](int sig, const std::string& name) {
            TestCallback(sig, name);
        });
    
    EXPECT_TRUE(success);
    
    // 检查常用关闭信号是否已注册
    auto signals = handler.getRegisteredSignals();
    EXPECT_TRUE(std::find(signals.begin(), signals.end(), SIGINT) != signals.end());
    EXPECT_TRUE(std::find(signals.begin(), signals.end(), SIGTERM) != signals.end());
    EXPECT_TRUE(std::find(signals.begin(), signals.end(), SIGQUIT) != signals.end());
}

// 测试关闭状态管理
TEST_F(SignalHandlerTest, ShutdownStateManagement) {
    auto& handler = SignalHandler::getInstance();
    
    // 初始状态应该是未关闭
    EXPECT_FALSE(handler.isShutdownRequested());
    
    // 注册关闭处理器
    handler.registerGracefulShutdown(
        [this](int sig, const std::string& name) {
            TestCallback(sig, name);
        });
    
    // 发送SIGINT信号
    std::raise(SIGINT);
    std::this_thread::sleep_for(100ms);
    
    // 应该设置关闭标志
    EXPECT_TRUE(handler.isShutdownRequested());
    EXPECT_TRUE(callback_called_.load());
    
    // 重置状态
    handler.reset();
    EXPECT_FALSE(handler.isShutdownRequested());
}

// 测试多重回调
TEST_F(SignalHandlerTest, MultipleCallbacks) {
    auto& handler = SignalHandler::getInstance();
    
    std::atomic<int> callback1_count{0};
    std::atomic<int> callback2_count{0};
    
    // 为同一个信号注册多个回调
    handler.registerSignalHandler(SIGUSR1, 
        [&callback1_count](int sig, const std::string& name) {
            callback1_count++;
        }, "SIGUSR1");
    
    handler.registerSignalHandler(SIGUSR1, 
        [&callback2_count](int sig, const std::string& name) {
            callback2_count++;
        }, "SIGUSR1");
    
    // 发送信号
    std::raise(SIGUSR1);
    std::this_thread::sleep_for(100ms);
    
    // 两个回调都应该被调用
    EXPECT_EQ(callback1_count.load(), 1);
    EXPECT_EQ(callback2_count.load(), 1);
}

// 测试信号名称默认生成
TEST_F(SignalHandlerTest, DefaultSignalNames) {
    auto& handler = SignalHandler::getInstance();
    
    // 不提供信号名称
    handler.registerSignalHandler(SIGUSR2, 
        [this](int sig, const std::string& name) {
            TestCallback(sig, name);
        });
    
    std::raise(SIGUSR2);
    std::this_thread::sleep_for(100ms);
    
    EXPECT_TRUE(callback_called_.load());
    EXPECT_EQ(callback_name_, "SIGUSR2");  // 应该自动生成名称
}

// 测试异常处理
TEST_F(SignalHandlerTest, ExceptionHandling) {
    auto& handler = SignalHandler::getInstance();
    
    std::atomic<bool> callback2_called{false};
    
    // 注册一个会抛出异常的回调
    handler.registerSignalHandler(SIGUSR1, 
        [](int sig, const std::string& name) {
            throw std::runtime_error("Test exception");
        }, "SIGUSR1");
    
    // 注册另一个正常的回调
    handler.registerSignalHandler(SIGUSR1, 
        [&callback2_called](int sig, const std::string& name) {
            callback2_called = true;
        }, "SIGUSR1");
    
    // 发送信号
    std::raise(SIGUSR1);
    std::this_thread::sleep_for(100ms);
    
    // 第二个回调应该仍然被调用（异常被捕获）
    EXPECT_TRUE(callback2_called.load());
}

// 测试清理功能
TEST_F(SignalHandlerTest, CleanupFunctionality) {
    auto& handler = SignalHandler::getInstance();
    
    // 注册一些信号处理器
    handler.registerSignalHandler(SIGUSR1, 
        [this](int sig, const std::string& name) {
            TestCallback(sig, name);
        }, "SIGUSR1");
    
    handler.registerGracefulShutdown(
        [this](int sig, const std::string& name) {
            TestCallback(sig, name);
        });
    
    // 验证信号已注册
    auto signals_before = handler.getRegisteredSignals();
    EXPECT_FALSE(signals_before.empty());
    
    // 清理
    handler.cleanup();
    
    // 验证信号已清理
    auto signals_after = handler.getRegisteredSignals();
    EXPECT_TRUE(signals_after.empty());
}

// 测试 ScopedSignalHandler RAII 包装器
TEST_F(SignalHandlerTest, ScopedSignalHandler) {
    std::atomic<bool> callback_called{false};
    
    {
        ScopedSignalHandler scoped_handler([&callback_called](int sig, const std::string& name) {
            callback_called = true;
        });
        
        // 发送信号
        std::raise(SIGINT);
        std::this_thread::sleep_for(100ms);
        
        EXPECT_TRUE(callback_called.load());
        EXPECT_TRUE(scoped_handler.isShutdownRequested());
        
    } // scoped_handler 析构，应该自动清理
    
    // 验证清理完成
    auto& handler = SignalHandler::getInstance();
    auto signals = handler.getRegisteredSignals();
    EXPECT_TRUE(signals.empty());
}

// 测试等待关闭功能
TEST_F(SignalHandlerTest, WaitForShutdown) {
    auto& handler = SignalHandler::getInstance();
    
    handler.registerGracefulShutdown(
        [this](int sig, const std::string& name) {
            TestCallback(sig, name);
        });
    
    // 在另一个线程中发送信号
    std::thread signal_thread([&handler]() {
        std::this_thread::sleep_for(200ms);
        std::raise(SIGINT);
    });
    
    // 等待关闭信号（应该在200ms后收到）
    auto start_time = std::chrono::steady_clock::now();
    handler.waitForShutdown();
    auto end_time = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 验证等待时间合理（大约200ms）
    EXPECT_GE(duration.count(), 180);  // 至少180ms
    EXPECT_LE(duration.count(), 500);  // 不超过500ms
    
    EXPECT_TRUE(handler.isShutdownRequested());
    EXPECT_TRUE(callback_called_.load());
    
    signal_thread.join();
}

// 测试线程安全性
TEST_F(SignalHandlerTest, ThreadSafety) {
    auto& handler = SignalHandler::getInstance();
    
    std::atomic<int> total_callbacks{0};
    const int num_threads = 10;
    const int signals_per_thread = 5;
    
    // 注册信号处理器
    handler.registerSignalHandler(SIGUSR1, 
        [&total_callbacks](int sig, const std::string& name) {
            total_callbacks++;
        }, "SIGUSR1");
    
    std::vector<std::thread> threads;
    
    // 创建多个线程同时发送信号
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([signals_per_thread]() {
            for (int j = 0; j < signals_per_thread; ++j) {
                std::raise(SIGUSR1);
                std::this_thread::sleep_for(10ms);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 等待信号处理完成
    std::this_thread::sleep_for(500ms);
    
    // 验证所有回调都被调用了
    EXPECT_EQ(total_callbacks.load(), num_threads * signals_per_thread);
}

// 性能测试
TEST_F(SignalHandlerTest, PerformanceTest) {
    auto& handler = SignalHandler::getInstance();
    
    std::atomic<int> callback_count{0};
    
    handler.registerSignalHandler(SIGUSR1, 
        [&callback_count](int sig, const std::string& name) {
            callback_count++;
        }, "SIGUSR1");
    
    const int num_signals = 1000;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 发送大量信号
    for (int i = 0; i < num_signals; ++i) {
        std::raise(SIGUSR1);
    }
    
    // 等待处理完成
    while (callback_count.load() < num_signals) {
        std::this_thread::sleep_for(1ms);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 验证性能（应该在合理时间内完成）
    EXPECT_LT(duration.count(), 5000);  // 不超过5秒
    EXPECT_EQ(callback_count.load(), num_signals);
    
    std::cout << "Processed " << num_signals << " signals in " << duration.count() << "ms" << std::endl;
}

// 边界测试
TEST_F(SignalHandlerTest, EdgeCases) {
    auto& handler = SignalHandler::getInstance();
    
    // 测试无效信号（这个测试在某些系统上可能会失败）
    // bool success = handler.registerSignalHandler(999, 
    //     [](int sig, const std::string& name) {}, "INVALID");
    // EXPECT_FALSE(success);  // 可能成功也可能失败，取决于系统
    
    // 测试空回调
    bool success = handler.registerSignalHandler(SIGUSR1, nullptr, "SIGUSR1");
    EXPECT_TRUE(success);  // 应该允许空回调
    
    // 测试重复清理
    handler.cleanup();
    handler.cleanup();  // 应该不会崩溃
    
    // 测试重复重置
    handler.reset();
    handler.reset();  // 应该不会崩溃
}

// 集成测试：模拟真实使用场景
TEST_F(SignalHandlerTest, RealWorldScenario) {
    auto& handler = SignalHandler::getInstance();
    
    std::atomic<bool> server_running{true};
    std::atomic<bool> config_reloaded{false};
    std::atomic<bool> stats_printed{false};
    
    // 注册优雅关闭
    handler.registerGracefulShutdown([&server_running](int sig, const std::string& name) {
        std::cout << "Server shutting down due to " << name << std::endl;
        server_running = false;
    });
    
    // 注册配置重载
    handler.registerSignalHandler(SIGUSR1, [&config_reloaded](int sig, const std::string& name) {
        std::cout << "Reloading configuration..." << std::endl;
        config_reloaded = true;
    }, "SIGUSR1");
    
    // 注册统计信息打印
    handler.registerSignalHandler(SIGUSR2, [&stats_printed](int sig, const std::string& name) {
        std::cout << "Printing statistics..." << std::endl;
        stats_printed = true;
    }, "SIGUSR2");
    
    // 模拟服务器运行
    std::thread server_thread([&server_running, &handler]() {
        int counter = 0;
        while (server_running && !handler.isShutdownRequested() && counter < 50) {
            std::this_thread::sleep_for(10ms);
            counter++;
        }
    });
    
    // 发送各种信号
    std::this_thread::sleep_for(50ms);
    std::raise(SIGUSR1);  // 重载配置
    
    std::this_thread::sleep_for(50ms);
    std::raise(SIGUSR2);  // 打印统计
    
    std::this_thread::sleep_for(50ms);
    std::raise(SIGTERM);  // 关闭服务器
    
    server_thread.join();
    
    // 验证所有信号都被正确处理
    EXPECT_TRUE(config_reloaded.load());
    EXPECT_TRUE(stats_printed.load());
    EXPECT_FALSE(server_running.load());
    EXPECT_TRUE(handler.isShutdownRequested());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}