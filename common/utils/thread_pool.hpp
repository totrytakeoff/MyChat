#pragma

#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

/******************************************************************************
 *
 * @file       thread_pool.hpp
 * @brief      线程池
 *
 * @author     myself
 * @date       2025/07/14
 *
 *****************************************************************************/

#include "singleton.hpp"
// #include "logger.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace im {
namespace utils {

class ThreadPool : public Singleton<ThreadPool> {
    friend class Singleton<ThreadPool>;

public:
    ~ThreadPool();

    void Init(size_t threadCount =
                      std::thread::hardware_concurrency());  // 初始化线程池，默认线程数为硬件并发数

    void Shutdown();  // 关闭线程池

    /**
     * @brief 提交任务到线程池
     *
     * 这是一个可变参数模板函数，可以接受任意类型的函数和参数
     *
     * @tparam F 函数类型（可以是函数指针、函数对象、lambda等）
     * @tparam Args 参数包，表示任意数量的参数类型
     * @param f 要执行的函数（使用完美转发）
     * @param args 函数参数（使用完美转发）
     * @return std::future<return_type> 用于获取任务执行结果的future对象
     *
     * 高级语法说明：
     * 1. template<class F, class... Args> - 可变参数模板
     * 2. F&& f, Args&&... args - 完美转发，保持参数的原始值类型
     * 3. auto -> std::future<...> - 尾置返回类型
     * 4. std::result_of<F(Args...)>::type - 类型萃取，获取函数返回类型
     */

    template <class F, class... Args>
    auto Enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    size_t GetThreadCount() const { return m_threads.size(); }

    size_t GetTaskCount() const;

    bool IsShutdown() const { return m_shutdown.load(); }

private:
    ThreadPool() ;                             // 构造函数设为默认，防止外
    ThreadPool(const ThreadPool&) = delete;             // 禁止拷贝构造
    ThreadPool& operator=(const ThreadPool&) = delete;  // 禁止拷贝

    void WorkerThread();  // 工作线程函数，处理任务队列中的任务

    std::vector<std::thread> m_threads;         // 线程池中的线程
    std::queue<std::function<void()>> m_tasks;  // 任务队列
    mutable std::mutex m_mutex;                 // 互斥锁，保护任务队列
    std::condition_variable m_condition;        // 条件变量，用于线程同步
    std::atomic<bool> m_shutdown{false};        // 线程池是否关闭标志
    std::atomic<size_t> m_tasksCount{0};        // 线程池中的线程数量
};

/**
 * @brief Enqueue 方法的实现（异步）
 *
 * 这个函数展示了现代 C++ 的多个高级特性：
 * 1. 可变参数模板 (Variadic Templates)
 * 2. 完美转发 (Perfect Forwarding)
 * 3. 类型萃取 (Type Traits)
 * 4. 智能指针和 RAII
 * 5. Lambda 表达式
 * 6. 异常安全
 */

template <class F, class... Args>
auto ThreadPool::Enqueue(F&& f,
                         Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    // 使用类型萃取获取函数的返回类型
    // std::result_of<F(Args...)>::type 会在编译时推导出函数调用的返回类型
    // 例如：如果 F 是 int(int, double)，Args 是 int, double
    // 那么 return_type 就是 int
    using return_type = typename std::result_of<F(Args...)>::type;  // 获取函数返回类型

    // 创建 packaged_task 来包装函数调用
    // std::packaged_task<return_type()> 表示一个无参数、返回 return_type 的函数对象
    // std::bind 将函数 f 和参数 args 绑定成一个无参数的函数对象
    // std::forward 进行完美转发，保持参数的原始值类型（左值/右值）
    auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));


    // 获取与 packaged_task 关联的 future 对象
    // 调用者可以通过这个 future 获取任务执行结果
    std::future<return_type> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(m_mutex);  // 锁住任务队列，防止并发访问
        if (m_shutdown) {
            throw std::runtime_error("ThreadPool is shutdown, cannot enqueue new tasks.");
        }

        // 将任务添加到队列
        // [task]() { (*task)(); } 是一个 lambda 表达式
        // [task] 表示值捕获，将 task（shared_ptr）复制到 lambda 中
        // (*task)() 调用 packaged_task 的 operator()
        m_tasks.emplace([task]() { (*task)(); });  // 将任务添加到队列中
        m_tasksCount++;                            // 增加任务计数
    }

    m_condition.notify_one();  // 通知一个等待的线程，有新任务可处理
    return result;  // 返回 future 对象，调用者可以等待任务完成并获取结果
}

} // namespace utils
} // namespace im

#endif  // THREAD_POOL_HPP
