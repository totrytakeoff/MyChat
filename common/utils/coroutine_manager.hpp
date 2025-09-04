#ifndef COROUTINE_MANAGER_HPP
#define COROUTINE_MANAGER_HPP

/*************************************************************************
 * @file   coroutine_manager.hpp
 * @brief  协程管理器，提供统一的协程操作接口
 * 
 * @details 本文件实现了基于C++20的协程管理器，包含：
 *          1. Task协程任务类型 - 支持值返回和异常处理
 *          2. 协程调度器 - 集成线程池调度协程执行
 *          3. Awaitable包装器 - 包装std::future等异步操作
 *          4. 统一的协程管理接口
 * 
 * @author myself
 * @date   2025/08/29
 *************************************************************************/

#include <coroutine>
#include <exception>
#include <chrono>
#include <future>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <utility>

#include "singleton.hpp"
#include "thread_pool.hpp"

namespace im::common {
namespace utils = im::utils;

/**
 * @brief 协程任务类型，支持值返回和异常处理
 * 
 * @tparam T 返回值类型
 * 
 * @details Task是协程的核心类型，它包装了一个协程句柄并提供：
 *          1. 值返回机制 - 通过promise_type的return_value方法
 *          2. 异常处理 - 通过promise_type的unhandled_exception方法
 *          3. 移动语义 - 支持协程对象的移动而不支持拷贝
 *          4. 资源管理 - 自动管理协程句柄的生命周期
 * 
 * @note Task对象在析构时会自动销毁关联的协程句柄
 */
template<typename T>
struct Task {
    /**
     * @brief 协程承诺类型，定义协程的行为
     * 
     * @details promise_type决定了协程如何启动、如何返回值、如何处理异常等
     *          1. initial_suspend - 控制协程是否立即开始执行
     *          2. final_suspend - 控制协程结束后是否立即销毁
     *          3. return_value - 处理协程的返回值
     *          4. unhandled_exception - 处理协程中的未捕获异常
     */
    struct promise_type {
        T value;                                    ///< 协程返回值
        std::exception_ptr exception;               ///< 异常指针，用于异常处理
        std::coroutine_handle<> continuation_;      ///< 等待此协程完成的协程句柄
        
        /**
         * @brief 获取协程对象
         * @return Task 协程对象
         */
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        /**
         * @brief 初始挂起点，控制协程是否立即执行
         * @return std::suspend_never 立即执行，不挂起
         */
        std::suspend_never initial_suspend() { return {}; }
        
        /**
         * @brief 最终挂起点，控制协程结束后的行为
         * @return std::suspend_always 协程结束后挂起，等待销毁
         */
        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto& promise = h.promise();
                    if (promise.continuation_) {
                        return promise.continuation_;
                    }
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }
        
        /**
         * @brief 处理协程返回值
         * @param v 返回值
         */
        void return_value(T v) {
            value = std::move(v);
        }
        
        /**
         * @brief 处理协程中的未捕获异常
         */
        void unhandled_exception() {
            exception = std::current_exception();
        }
    };
    
    std::coroutine_handle<promise_type> handle;  ///< 协程句柄
    
    /**
     * @brief 构造函数
     * @param h 协程句柄
     */
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    
    /**
     * @brief 析构函数，自动销毁协程句柄
     */
    ~Task() {
        if (handle) handle.destroy();
    }
    
    // 禁用拷贝构造和拷贝赋值
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    /**
     * @brief 移动构造函数
     * @param other 另一个Task对象
     */
    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    
    /**
     * @brief 移动赋值操作符
     * @param other 另一个Task对象
     * @return Task& 引用
     */
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief 检查协程是否已经完成
     * @return true 如果协程已经完成
     * @return false 如果协程未完成
     */
    bool await_ready() const noexcept {
        return handle.done();
    }
    
    /**
     * @brief 挂起当前协程并设置continuation
     * @param continuation 等待此协程完成的协程句柄
     */
    void await_suspend(std::coroutine_handle<> continuation) noexcept {
        handle.promise().continuation_ = continuation;
        handle.resume();
    }
    
    /**
     * @brief 获取协程结果
     * @return T 协程返回值
     * @throws 重新抛出协程中的异常
     */
    T await_resume() {
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
        return std::move(handle.promise().value);
    }
    
    /**
     * @brief 获取协程结果（保持向后兼容）
     * @return T 协程返回值
     * @throws 重新抛出协程中的异常
     */
    T get() {
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
        return std::move(handle.promise().value);
    }
};

/**
 * @brief 特化版本：处理void返回类型的协程
 */
template<>
struct Task<void> {
    struct promise_type {
        std::exception_ptr exception;
        std::coroutine_handle<> continuation_;      ///< 等待此协程完成的协程句柄
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        
        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto& promise = h.promise();
                    if (promise.continuation_) {
                        return promise.continuation_;
                    }
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }
        
        void return_void() {}
        
        void unhandled_exception() {
            exception = std::current_exception();
        }
    };
    
    std::coroutine_handle<promise_type> handle;
    
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    
    ~Task() {
        if (handle) handle.destroy();
    }
    
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief 检查协程是否已经完成
     * @return true 如果协程已经完成
     * @return false 如果协程未完成
     */
    bool await_ready() const noexcept {
        return handle.done();
    }
    
    /**
     * @brief 挂起当前协程并设置continuation
     * @param continuation 等待此协程完成的协程句柄
     */
    void await_suspend(std::coroutine_handle<> continuation) noexcept {
        handle.promise().continuation_ = continuation;
        handle.resume();
    }
    
    /**
     * @brief 获取协程结果
     * @throws 重新抛出协程中的异常
     */
    void await_resume() {
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
    }
    
    /**
     * @brief 获取协程结果（保持向后兼容）
     * @throws 重新抛出协程中的异常
     */
    void get() {
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
    }
};

/**
 * @brief Future包装器，使std::future可等待
 * 
 * @tparam T Future的值类型
 * 
 * @details 将std::future包装为可等待对象，使其可以在协程中使用co_await
 */
template<typename T>
struct FutureAwaiter {
    std::future<T> future;  ///< 被包装的future对象
    
    /**
     * @brief 构造函数
     * @param f future对象
     */
    explicit FutureAwaiter(std::future<T>&& f) : future(std::move(f)) {}
    
    /**
     * @brief 检查future是否已经就绪
     * @return true 如果future已经就绪
     * @return false 如果future未就绪，需要挂起协程
     */
    bool await_ready() { 
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; 
    }
    
    /**
     * @brief 挂起协程并在后台等待future完成
     * @param handle 协程句柄
     */
    void await_suspend(std::coroutine_handle<> handle) {
        // 在新线程中等待future完成，完成后恢复协程
        std::thread([handle, future = std::move(this->future)]() mutable {
            future.wait();
            handle.resume();
        }).detach();
    }
    
    /**
     * @brief 获取future的结果
     * @return T future的结果值
     */
    T await_resume() { 
        return future.get(); 
    }
};

/**
 * @brief 延迟等待器，支持协程中的延迟操作
 */
struct DelayAwaiter {
    std::chrono::milliseconds duration;  ///< 延迟时间
    
    /**
     * @brief 构造函数
     * @param ms 延迟毫秒数
     */
    explicit DelayAwaiter(std::chrono::milliseconds ms) : duration(ms) {}
    
    /**
     * @brief 检查是否需要延迟
     * @return true 如果不需要延迟（duration为0）
     * @return false 如果需要延迟
     */
    bool await_ready() { 
        return duration.count() == 0; 
    }
    
    /**
     * @brief 挂起协程并延迟执行
     * @param handle 协程句柄
     */
    void await_suspend(std::coroutine_handle<> handle) {
        std::thread([handle, duration = this->duration]() {
            std::this_thread::sleep_for(duration);
            handle.resume();
        }).detach();
    }
    
    /**
     * @brief 延迟完成后执行
     */
    void await_resume() {}
};

/**
 * @brief 协程管理器，提供统一的协程操作接口
 * 
 * @details 协程管理器是单例类，提供：
 *          1. 协程调度功能 - 集成线程池调度协程
 *          2. Awaitable包装 - 包装异步操作为可等待对象
 *          3. 统一接口 - 简化协程的使用
 */
class CoroutineManager : public utils::Singleton<CoroutineManager> {
    friend class utils::Singleton<CoroutineManager>;
    
private:
    std::shared_ptr<utils::ThreadPool> thread_pool_;  ///< 线程池，用于调度协程
    
    /**
     * @brief 构造函数
     */
    CoroutineManager();
    
public:
    /**
     * @brief 获取协程管理器实例
     * @return CoroutineManager& 协程管理器引用
     */
    static CoroutineManager& getInstance() {
        return utils::Singleton<CoroutineManager>::GetInstance();
    }
    
    /**
     * @brief 调度协程在指定线程池中执行
     * @tparam T 协程返回值类型
     * @param task 协程任务
     * @param pool 线程池（可选，默认使用内部线程池）
     */
    template<typename T>
    void schedule(Task<T>&& task, std::shared_ptr<utils::ThreadPool> pool = nullptr);
    
    /**
     * @brief 包装std::future为可等待对象
     * @tparam T Future值类型
     * @param future std::future对象
     * @return FutureAwaiter<T> 可等待对象
     */
    template<typename T>
    static FutureAwaiter<T> make_future_awaiter(std::future<T>&& future);
    
    /**
     * @brief 创建延迟等待器
     * @param ms 延迟毫秒数
     * @return DelayAwaiter 延迟等待器
     */
    static DelayAwaiter make_delay_awaiter(std::chrono::milliseconds ms);
};

// 为std::future提供co_await支持
template<typename T>
auto operator co_await(std::future<T>&& future) {
    return CoroutineManager::make_future_awaiter(std::move(future));
}

// 为延迟操作提供字面量支持
inline DelayAwaiter operator""_ms(unsigned long long ms) {
    return CoroutineManager::make_delay_awaiter(std::chrono::milliseconds{ms});
}

} // namespace im::common

#endif // COROUTINE_MANAGER_HPP