# 协程管理类设计文档

## 1. 整体架构

协程管理类将包含以下核心组件：
- `Task<T>`: 协程任务类型封装
- `Promise<T>`: 协程承诺类型
- `CoroutineScheduler`: 协程调度器/线程池
- `AwaitableWrapper`: 异步操作包装器

## 2. Task协程类型定义

```cpp
#include <coroutine>
#include <exception>
#include <atomic>

template<typename T>
struct Task {
    struct promise_type {
        T value;
        std::exception_ptr exception;
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(T v) {
            value = std::move(v);
        }
        
        void unhandled_exception() {
            exception = std::current_exception();
        }
    };
    
    std::coroutine_handle<promise_type> handle;
    
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    
    ~Task() {
        if (handle) handle.destroy();
    }
    
    // 禁用拷贝，支持移动
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : handle(std::exchange(other.handle, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }
    
    T get() {
        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
        return std::move(handle.promise().value);
    }
};
```

## 3. 协程调度器设计

```cpp
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class CoroutineScheduler {
public:
    explicit CoroutineScheduler(size_t thread_count = std::thread::hardware_concurrency())
        : stop_(false) {
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        
                        if (stop_ && tasks_.empty()) return;
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }
    
    // 调度协程句柄
    template<typename T>
    void schedule(std::coroutine_handle<T> handle) {
        {
            std::scoped_lock lock(queue_mutex_);
            tasks_.emplace([handle]() { handle.resume(); });
        }
        condition_.notify_one();
    }
    
    // 调度普通函数
    template<typename F>
    void schedule(F&& f) {
        {
            std::scoped_lock lock(queue_mutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }
    
    ~CoroutineScheduler() {
        {
            std::scoped_lock lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        
        for (std::thread& worker : workers_) {
            worker.join();
        }
    }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};
```

## 4. Awaitable包装器

```cpp
#include <future>
#include <chrono>

// Future包装器
template<typename T>
struct FutureAwaiter {
    std::future<T> future;
    
    FutureAwaiter(std::future<T>&& f) : future(std::move(f)) {}
    
    bool await_ready() { return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
    
    void await_suspend(std::coroutine_handle<> handle) {
        // 在线程池中等待future完成
        std::thread([handle, future = std::move(this->future)]() mutable {
            future.wait();
            handle.resume();
        }).detach();
    }
    
    T await_resume() { return future.get(); }
};

// 用于包装std::future
template<typename T>
auto operator co_await(std::future<T>&& future) {
    return FutureAwaiter<T>{std::move(future)};
}

// Delay包装器
struct DelayAwaiter {
    std::chrono::milliseconds duration;
    
    DelayAwaiter(std::chrono::milliseconds ms) : duration(ms) {}
    
    bool await_ready() { return duration.count() == 0; }
    
    void await_suspend(std::coroutine_handle<> handle) {
        std::thread([handle, duration = this->duration]() {
            std::this_thread::sleep_for(duration);
            handle.resume();
        }).detach();
    }
    
    void await_resume() {}
};

// 用于延迟执行
inline DelayAwaiter operator""_ms(unsigned long long ms) {
    return DelayAwaiter{std::chrono::milliseconds{ms}};
}
```

## 5. 使用示例

```cpp
// 异步认证操作
Task<bool> async_authenticate(const std::string& token) {
    // 模拟异步操作
    auto future = std::async(std::launch::async, [token]() {
        // 实际认证逻辑
        return token == "valid_token";
    });
    
    bool result = co_await future;
    co_return result;
}

// 异步路由查找
Task<ProcessorFunction> async_find_processor(uint32_t cmd_id) {
    // 模拟异步路由查找
    auto future = std::async(std::launch::async, [cmd_id]() {
        // 实际路由查找逻辑
        return get_processor_from_router(cmd_id);
    });
    
    auto processor = co_await future;
    co_return processor;
}
```