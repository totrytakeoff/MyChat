#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

/******************************************************************************
 *
 * @file       signal_handler.hpp
 * @brief      通用信号处理器 - 支持优雅关闭和自定义信号回调
 *             提供跨平台的信号处理功能，支持回调机制
 *
 * @details    主要功能：
 *             - 支持多种信号类型（SIGINT、SIGTERM、SIGQUIT等）
 *             - 回调机制，提高灵活性和可复用性
 *             - 线程安全的信号状态管理
 *             - 优雅关闭支持
 *             - 支持信号链式处理
 *
 * @author     myself
 * @date       2025/09/18
 * @version    1.0
 *
 *****************************************************************************/

#include <csignal>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace im {
namespace utils {

/**
 * @brief 信号处理器回调函数类型
 * @param signal 信号编号
 * @param signal_name 信号名称
 */
using SignalCallback = std::function<void(int signal, const std::string& signal_name)>;

/**
 * @brief 通用信号处理器类
 * 
 * @details 提供灵活的信号处理机制，支持：
 *          - 多信号类型注册
 *          - 回调函数机制
 *          - 优雅关闭支持
 *          - 线程安全操作
 * 
 * @note 使用单例模式确保全局唯一性
 */
class SignalHandler {
public:
    /**
     * @brief 获取信号处理器实例（单例模式）
     */
    static SignalHandler& getInstance() {
        static SignalHandler instance;
        return instance;
    }

    /**
     * @brief 注册信号回调处理器
     * @param signal 信号类型（如 SIGINT, SIGTERM）
     * @param callback 回调函数
     * @param signal_name 信号名称（可选，用于日志）
     * @return 注册是否成功
     */
    bool registerSignalHandler(int signal, SignalCallback callback, 
                              const std::string& signal_name = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            // 存储回调函数
            callbacks_[signal].push_back(callback);
            
            // 存储信号名称
            if (!signal_name.empty()) {
                signal_names_[signal] = signal_name;
            } else {
                signal_names_[signal] = getDefaultSignalName(signal);
            }
            
            // 注册系统信号处理器
            if (std::signal(signal, signalHandler) == SIG_ERR) {
                callbacks_[signal].pop_back(); // 回滚
                return false;
            }
            
            return true;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief 注册常用的优雅关闭信号（SIGINT, SIGTERM, SIGQUIT）
     * @param callback 关闭回调函数
     * @return 注册是否成功
     */
    bool registerGracefulShutdown(SignalCallback callback) {
        bool success = true;
        success &= registerSignalHandler(SIGINT, callback, "SIGINT");
        success &= registerSignalHandler(SIGTERM, callback, "SIGTERM");
        success &= registerSignalHandler(SIGQUIT, callback, "SIGQUIT");
        
        // 忽略 SIGPIPE（断开的管道）
        std::signal(SIGPIPE, SIG_IGN);
        
        return success;
    }

    /**
     * @brief 检查是否收到关闭信号
     */
    bool isShutdownRequested() const {
        return shutdown_requested_.load();
    }

    /**
     * @brief 等待关闭信号
     * @param message 等待时显示的消息
     */
    void waitForShutdown(const std::string& message = "Press Ctrl+C to shutdown...") {
        if (!message.empty()) {
            std::cout << message << std::endl;
        }

        // Busy-wait with sleep until a shutdown signal is received
        while (!shutdown_requested_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Safely copy callbacks for the last received signal and invoke them in a safe context
        std::vector<SignalCallback> callbacks_copy;
        int signal_number = last_signal_.load();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = callbacks_.find(signal_number);
            if (it != callbacks_.end()) {
                callbacks_copy = it->second;
            }
        }

        const std::string signal_name = getDefaultSignalName(signal_number);
        for (const auto& callback : callbacks_copy) {
            try {
                callback(signal_number, signal_name);
            } catch (const std::exception& e) {
                std::cerr << "Error in signal callback: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown error in signal callback" << std::endl;
            }
        }
    }

    /**
     * @brief 重置关闭状态（主要用于测试）
     */
    void reset() {
        shutdown_requested_.store(false);
    }

    /**
     * @brief 清理信号处理器
     */
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 恢复默认信号处理器
        for (const auto& [signal, _] : callbacks_) {
            std::signal(signal, SIG_DFL);
        }
        
        callbacks_.clear();
        signal_names_.clear();
    }

    /**
     * @brief 获取已注册的信号列表
     */
    std::vector<int> getRegisteredSignals() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<int> signals;
        for (const auto& [signal, _] : callbacks_) {
            signals.push_back(signal);
        }
        return signals;
    }

private:
    SignalHandler() : shutdown_requested_(false) {}
    ~SignalHandler() { cleanup(); }

    // 禁用拷贝和赋值
    SignalHandler(const SignalHandler&) = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;

    /**
     * @brief 系统信号处理器函数
     */
    static void signalHandler(int signal) {
        // Async-signal-safe handler: only set flags/state; do not lock, allocate, or IO
        auto& instance = getInstance();
        instance.last_signal_.store(signal);
        if (signal == SIGINT || signal == SIGTERM || signal == SIGQUIT) {
            instance.shutdown_requested_.store(true);
        }
    }

    /**
     * @brief 获取信号的默认名称
     */
    std::string getDefaultSignalName(int signal) {
        switch (signal) {
            case SIGINT:  return "SIGINT";
            case SIGTERM: return "SIGTERM";
            case SIGQUIT: return "SIGQUIT";
            case SIGHUP:  return "SIGHUP";
            case SIGUSR1: return "SIGUSR1";
            case SIGUSR2: return "SIGUSR2";
            default:      return "SIG" + std::to_string(signal);
        }
    }

private:
    mutable std::mutex mutex_;
    std::atomic<bool> shutdown_requested_;
    std::atomic<int> last_signal_{0};
    std::unordered_map<int, std::vector<SignalCallback>> callbacks_;
    std::unordered_map<int, std::string> signal_names_;
};

/**
 * @brief 信号处理器便利类 - RAII方式管理信号处理器
 */
class ScopedSignalHandler {
public:
    explicit ScopedSignalHandler(SignalCallback callback) {
        SignalHandler::getInstance().registerGracefulShutdown(std::move(callback));
    }
    
    ~ScopedSignalHandler() {
        SignalHandler::getInstance().cleanup();
    }

    bool isShutdownRequested() const {
        return SignalHandler::getInstance().isShutdownRequested();
    }

    void waitForShutdown(const std::string& message = "") const {
        SignalHandler::getInstance().waitForShutdown(message);
    }

private:
    ScopedSignalHandler(const ScopedSignalHandler&) = delete;
    ScopedSignalHandler& operator=(const ScopedSignalHandler&) = delete;
};

}  // namespace utils
}  // namespace im

#endif  // SIGNAL_HANDLER_HPP