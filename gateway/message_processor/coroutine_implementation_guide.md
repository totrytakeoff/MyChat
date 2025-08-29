# Modern C++20 Coroutine Patterns for Task Management

Based on the MyChat codebase analysis, this guide provides comprehensive examples for implementing modern C++20 coroutine patterns for task management.

## 1. Promise Type Design for Custom Task Types

A well-designed promise type is crucial for creating a custom coroutine task type. Here's a complete implementation:

```cpp
#include <coroutine>
#include <exception>
#include <utility>

// Custom Task type with proper promise handling
template<typename T>
class Task {
public:
    // Promise type that defines coroutine behavior
    struct promise_type {
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        // Handle return values
        template<typename U>
        void return_value(U&& value) {
            value_ = std::forward<U>(value);
        }
        
        // Handle exceptions
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        // Create the coroutine handle
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        T value_;
        std::exception_ptr exception_;
    };
    
    // Constructor and destructor
    Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
    
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    // Move constructor and assignment
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    // Disable copy operations
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    // Get result (blocking)
    T get() {
        if (!handle_.done()) {
            // In a real implementation, you'd have an executor to resume on
            handle_.resume();
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        
        return std::move(handle_.promise().value_);
    }
    
private:
    std::coroutine_handle<promise_type> handle_;
};

// Specialization for void return type
template<>
class Task<void> {
public:
    struct promise_type {
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_void() {}
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::exception_ptr exception_;
    };
    
    Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
    
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    void get() {
        if (!handle_.done()) {
            handle_.resume();
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }
    
private:
    std::coroutine_handle<promise_type> handle_;
};
```

## 2. Coroutine Handle Management

Proper coroutine handle management is essential for memory safety and performance:

```cpp
#include <coroutine>
#include <memory>
#include <vector>

// Coroutine handle manager with reference counting
template<typename T>
class TaskManager {
public:
    // Shared coroutine handle with reference counting
    class SharedCoroutine {
    public:
        SharedCoroutine(std::coroutine_handle<typename Task<T>::promise_type> handle)
            : handle_(handle), ref_count_(new std::atomic<int>(1)) {}
        
        SharedCoroutine(const SharedCoroutine& other)
            : handle_(other.handle_), ref_count_(other.ref_count_) {
            ++(*ref_count_);
        }
        
        SharedCoroutine& operator=(const SharedCoroutine& other) {
            if (this != &other) {
                release();
                handle_ = other.handle_;
                ref_count_ = other.ref_count_;
                ++(*ref_count_);
            }
            return *this;
        }
        
        ~SharedCoroutine() {
            release();
        }
        
        std::coroutine_handle<typename Task<T>::promise_type> get() const {
            return handle_;
        }
        
        bool done() const {
            return handle_.done();
        }
        
        void resume() {
            if (!done()) {
                handle_.resume();
            }
        }
        
    private:
        void release() {
            if (--(*ref_count_) == 0) {
                handle_.destroy();
                delete ref_count_;
            }
        }
        
        std::coroutine_handle<typename Task<T>::promise_type> handle_;
        std::atomic<int>* ref_count_;
    };
    
    // Manage multiple coroutines
    void add_task(Task<T> task) {
        // Extract handle from task and store it
        auto handle = task.get_handle(); // Assuming we add this method to Task
        tasks_.emplace_back(handle);
    }
    
    void resume_all() {
        for (auto& task : tasks_) {
            if (!task.done()) {
                task.resume();
            }
        }
    }
    
private:
    std::vector<SharedCoroutine> tasks_;
};
```

## 3. Thread Pool Integration with Coroutines

Integrating coroutines with a thread pool for optimal performance:

```cpp
#include <coroutine>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>

// Executor that integrates with thread pool
class ThreadPoolExecutor {
public:
    explicit ThreadPoolExecutor(size_t thread_count = std::thread::hardware_concurrency())
        : stop_(false) {
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        
                        if (stop_ && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    ~ThreadPoolExecutor() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker : workers_) {
            worker.join();
        }
    }
    
    // Schedule a coroutine to run on the thread pool
    template<typename T>
    void schedule_coroutine(std::coroutine_handle<T> handle) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace([handle]() {
                if (!handle.done()) {
                    handle.resume();
                }
            });
        }
        condition_.notify_one();
    }
    
    // Schedule any callable
    template<typename F>
    void schedule(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

// Coroutine-aware task with executor support
template<typename T>
class ExecutorTask {
public:
    struct promise_type {
        ThreadPoolExecutor* executor_ = nullptr;
        
        std::suspend_always initial_suspend() { 
            return {}; 
        }
        
        std::suspend_always final_suspend() noexcept { 
            return {}; 
        }
        
        template<typename U>
        void return_value(U&& value) {
            value_ = std::forward<U>(value);
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        ExecutorTask get_return_object() {
            return ExecutorTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        // Set executor for this coroutine
        void set_executor(ThreadPoolExecutor* executor) {
            executor_ = executor;
        }
        
        auto await_transform(std::suspend_always) {
            struct Awaiter {
                ThreadPoolExecutor* executor_;
                
                bool await_ready() { return false; }
                
                void await_suspend(std::coroutine_handle<promise_type> handle) {
                    if (executor_) {
                        executor_->schedule_coroutine(handle);
                    } else {
                        // Resume immediately if no executor
                        handle.resume();
                    }
                }
                
                void await_resume() {}
            };
            return Awaiter{executor_};
        }
        
        T value_;
        std::exception_ptr exception_;
    };
    
    ExecutorTask(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
    
    ~ExecutorTask() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    ExecutorTask(ExecutorTask&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    
    ExecutorTask& operator=(ExecutorTask&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    ExecutorTask(const ExecutorTask&) = delete;
    ExecutorTask& operator=(const ExecutorTask&) = delete;
    
    T get() {
        if (!handle_.done()) {
            handle_.resume();
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        
        return std::move(handle_.promise().value_);
    }
    
    // Set executor for this task
    void set_executor(ThreadPoolExecutor* executor) {
        handle_.promise().set_executor(executor);
    }
    
private:
    std::coroutine_handle<promise_type> handle_;
};
```

## 4. Exception Handling in Coroutines

Proper exception handling in coroutines is critical for robust applications:

```cpp
#include <coroutine>
#include <exception>
#include <stdexcept>

// Enhanced Task with comprehensive exception handling
template<typename T>
class ExceptionSafeTask {
public:
    struct promise_type {
        std::exception_ptr exception_;
        T value_;
        
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        template<typename U>
        void return_value(U&& value) {
            try {
                value_ = std::forward<U>(value);
            } catch (...) {
                exception_ = std::current_exception();
            }
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        ExceptionSafeTask get_return_object() {
            return ExceptionSafeTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        // Custom awaitable for exception-safe operations
        template<typename Awaitable>
        auto await_transform(Awaitable&& awaitable) {
            struct ExceptionSafeAwaiter {
                decltype(awaitable) awaitable_;
                std::exception_ptr& exception_ref_;
                
                ExceptionSafeAwaiter(decltype(awaitable)&& a, std::exception_ptr& e) 
                    : awaitable_(std::forward<Awaitable>(a)), exception_ref_(e) {}
                
                auto await_ready() {
                    try {
                        return awaitable_.await_ready();
                    } catch (...) {
                        exception_ref_ = std::current_exception();
                        return true;
                    }
                }
                
                template<typename Promise>
                auto await_suspend(std::coroutine_handle<Promise> handle) {
                    try {
                        return awaitable_.await_suspend(handle);
                    } catch (...) {
                        exception_ref_ = std::current_exception();
                        return false; // Resume immediately to handle exception
                    }
                }
                
                auto await_resume() {
                    if (exception_ref_) {
                        std::rethrow_exception(exception_ref_);
                    }
                    try {
                        return awaitable_.await_resume();
                    } catch (...) {
                        exception_ref_ = std::current_exception();
                        throw;
                    }
                }
            };
            return ExceptionSafeAwaiter{std::forward<Awaitable>(awaitable), exception_};
        }
    };
    
    ExceptionSafeTask(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
    
    ~ExceptionSafeTask() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    ExceptionSafeTask(ExceptionSafeTask&& other) noexcept 
        : handle_(std::exchange(other.handle_, {})) {}
    
    ExceptionSafeTask& operator=(ExceptionSafeTask&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    ExceptionSafeTask(const ExceptionSafeTask&) = delete;
    ExceptionSafeTask& operator=(const ExceptionSafeTask&) = delete;
    
    T get() {
        if (!handle_.done()) {
            handle_.resume();
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        
        return std::move(handle_.promise().value_);
    }
    
private:
    std::coroutine_handle<promise_type> handle_;
};

// Usage example with exception handling
ExceptionSafeTask<int> example_with_exceptions() {
    try {
        // Some operation that might throw
        co_await std::suspend_always{};
        
        // Simulate an exception
        if (true) {
            throw std::runtime_error("Example exception");
        }
        
        co_return 42;
    } catch (const std::exception& e) {
        // Handle exception locally
        std::cout << "Caught exception in coroutine: " << e.what() << std::endl;
        co_return -1;
    }
}
```

## 5. Executor/Scheduler Patterns for Coroutines

Creating flexible executor/scheduler patterns for coroutine orchestration:

```cpp
#include <coroutine>
#include <memory>
#include <queue>
#include <chrono>
#include <functional>

// Base executor interface
class Executor {
public:
    virtual ~Executor() = default;
    virtual void execute(std::coroutine_handle<> handle) = 0;
    virtual bool is_running_on_current_thread() const = 0;
};

// Inline executor - executes immediately
class InlineExecutor : public Executor {
public:
    void execute(std::coroutine_handle<> handle) override {
        if (!handle.done()) {
            handle.resume();
        }
    }
    
    bool is_running_on_current_thread() const override {
        return true;
    }
};

// Thread pool executor
class ThreadPoolExecutorScheduler : public Executor {
public:
    explicit ThreadPoolExecutorScheduler(size_t thread_count = 4) {
        // Implementation similar to previous example
        // ...
    }
    
    void execute(std::coroutine_handle<> handle) override {
        // Schedule on thread pool
        // ...
    }
    
    bool is_running_on_current_thread() const override {
        // Check if current thread belongs to this pool
        return false;
    }
};

// Scheduled executor with delay support
class ScheduledExecutor : public Executor {
public:
    struct ScheduledTask {
        std::chrono::steady_clock::time_point execute_time;
        std::coroutine_handle<> handle;
        
        bool operator>(const ScheduledTask& other) const {
            return execute_time > other.execute_time;
        }
    };
    
    ScheduledExecutor() : stop_(false) {
        scheduler_thread_ = std::thread([this] { run_scheduler(); });
    }
    
    ~ScheduledExecutor() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        if (scheduler_thread_.joinable()) {
            scheduler_thread_.join();
        }
    }
    
    void execute(std::coroutine_handle<> handle) override {
        schedule_at(std::chrono::steady_clock::now(), handle);
    }
    
    void schedule_delayed(std::chrono::milliseconds delay, std::coroutine_handle<> handle) {
        auto execute_time = std::chrono::steady_clock::now() + delay;
        schedule_at(execute_time, handle);
    }
    
    void schedule_at(std::chrono::steady_clock::time_point time, std::coroutine_handle<> handle) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            task_queue_.push({time, handle});
        }
        condition_.notify_one();
    }
    
    bool is_running_on_current_thread() const override {
        return std::this_thread::get_id() == scheduler_thread_.get_id();
    }
    
private:
    void run_scheduler() {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { 
                return stop_ || !task_queue_.empty(); 
            });
            
            if (stop_ && task_queue_.empty()) {
                return;
            }
            
            auto now = std::chrono::steady_clock::now();
            if (!task_queue_.empty() && task_queue_.top().execute_time <= now) {
                auto task = task_queue_.top();
                task_queue_.pop();
                lock.unlock();
                
                if (!task.handle.done()) {
                    task.handle.resume();
                }
            } else if (!task_queue_.empty()) {
                auto next_time = task_queue_.top().execute_time;
                lock.unlock();
                condition_.wait_until(lock, next_time);
            }
        }
    }
    
    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>, std::greater<ScheduledTask>> task_queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread scheduler_thread_;
    bool stop_;
};

// Coroutine with executor support
template<typename T>
class ScheduledTask {
public:
    struct promise_type {
        Executor* executor_ = nullptr;
        std::exception_ptr exception_;
        T value_;
        
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        template<typename U>
        void return_value(U&& value) {
            value_ = std::forward<U>(value);
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
        
        ScheduledTask get_return_object() {
            return ScheduledTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        void set_executor(Executor* executor) {
            executor_ = executor;
        }
        
        // Custom awaiter that respects the executor
        auto await_transform(std::suspend_always) {
            struct ExecutorAwareAwaiter {
                Executor* executor_;
                
                bool await_ready() { return false; }
                
                void await_suspend(std::coroutine_handle<promise_type> handle) {
                    if (executor_) {
                        executor_->execute(handle);
                    } else {
                        // Execute inline if no executor set
                        handle.resume();
                    }
                }
                
                void await_resume() {}
            };
            return ExecutorAwareAwaiter{executor_};
        }
    };
    
    ScheduledTask(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
    
    ~ScheduledTask() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    ScheduledTask(ScheduledTask&& other) noexcept 
        : handle_(std::exchange(other.handle_, {})) {}
    
    ScheduledTask& operator=(ScheduledTask&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    ScheduledTask(const ScheduledTask&) = delete;
    ScheduledTask& operator=(const ScheduledTask&) = delete;
    
    T get() {
        if (!handle_.done()) {
            handle_.resume();
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        
        return std::move(handle_.promise().value_);
    }
    
    void set_executor(Executor* executor) {
        handle_.promise().set_executor(executor);
    }
    
private:
    std::coroutine_handle<promise_type> handle_;
};

// Usage example
ScheduledTask<int> example_with_scheduler() {
    // This would suspend and resume on the specified executor
    co_await std::suspend_always{};
    co_return 42;
}

// Example usage
void demonstrate_schedulers() {
    InlineExecutor inline_exec;
    ThreadPoolExecutorScheduler thread_exec(4);
    ScheduledExecutor scheduled_exec;
    
    // Create tasks with different executors
    auto task1 = example_with_scheduler();
    task1.set_executor(&inline_exec);
    
    auto task2 = example_with_scheduler();
    task2.set_executor(&thread_exec);
    
    auto task3 = example_with_scheduler();
    task3.set_executor(&scheduled_exec);
    
    // Schedule delayed execution
    scheduled_exec.schedule_delayed(std::chrono::milliseconds(100), task3.get_handle());
    
    // Get results
    auto result1 = task1.get();
    auto result2 = task2.get();
    // task3 will be executed after delay
}
```

## Complete Integration Example

Here's how these patterns would integrate in the MyChat system:

```cpp
// Integration with MyChat's existing components
class AsyncMessageProcessor {
private:
    std::shared_ptr<AuthManager> auth_mgr_;
    std::shared_ptr<RouterManager> router_mgr_;
    ThreadPoolExecutor executor_;
    
public:
    AsyncMessageProcessor(
        std::shared_ptr<AuthManager> auth_mgr,
        std::shared_ptr<RouterManager> router_mgr)
        : auth_mgr_(std::move(auth_mgr))
        , router_mgr_(std::move(router_mgr))
        , executor_(std::thread::hardware_concurrency()) {}

    // Coroutine-based message processing
    ExecutorTask<ProcessResult> process_message_coro(
        std::unique_ptr<UnifiedMessage> message) {
        
        // Set executor for this coroutine
        co_await std::suspend_always{}; // This will use our executor
        
        try {
            // 1. Authenticate user
            auto auth_task = verify_token_coro(message->get_token());
            auth_task.set_executor(&executor_);
            auto auth_result = co_await auth_task;
            
            if (!auth_result.success) {
                co_return ProcessResult::auth_failed(auth_result.error_msg);
            }
            
            // 2. Find service route
            auto route_task = find_service_coro(message->get_cmd_id());
            route_task.set_executor(&executor_);
            auto route_result = co_await route_task;
            
            if (!route_result.success) {
                co_return ProcessResult::route_failed(route_result.error_msg);
            }
            
            // 3. Call service
            auto service_task = call_service_coro(
                route_result.endpoint, 
                message->get_json_body());
            service_task.set_executor(&executor_);
            auto service_result = co_await service_task;
            
            // 4. Send response
            auto response_task = send_response_coro(*message, service_result);
            response_task.set_executor(&executor_);
            co_await response_task;
            
            co_return ProcessResult::success();
            
        } catch (const std::exception& e) {
            co_return ProcessResult::error(e.what());
        }
    }
    
private:
    ExecutorTask<AuthResult> verify_token_coro(const std::string& token) {
        co_await std::suspend_always{};
        
        auto future = std::async(std::launch::async, [this, token]() {
            UserTokenInfo user_info;
            bool success = auth_mgr_->verify_token(token, user_info);
            return AuthResult{success, user_info, success ? "" : "Invalid token"};
        });
        
        co_return future.get();
    }
    
    // Similar implementations for other methods...
};
```

This comprehensive guide covers all the key aspects of modern C++20 coroutine patterns for task management, providing practical examples that can be integrated into the MyChat system.