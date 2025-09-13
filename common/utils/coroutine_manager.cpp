#include "coroutine_manager.hpp"

namespace im::common {

/**
 * @brief 构造函数，初始化线程池
 */
CoroutineManager::CoroutineManager() 
    : thread_pool_(&utils::ThreadPool::GetInstance()) {
}

// getInstance() 已在头文件中内联实现

/**
 * @brief 调度协程在指定线程池中执行
 * @tparam T 协程返回值类型
 * @param task 协程任务
 * @param pool 线程池（可选，默认使用内部线程池）
 */
template<typename T>
void CoroutineManager::schedule(Task<T>&& task, std::shared_ptr<utils::ThreadPool> pool) {
    // Detach the coroutine handle from Task to avoid destruction on Task's dtor
    auto handle = task.handle;
    task.handle = nullptr;

    auto runner = [handle]() mutable {
        if (handle) {
            handle.resume();
            // 不在这里销毁，由上层拥有的 Task/promise 生命周期管理
        }
    };

    if (pool) {
        pool->Enqueue(runner);
    } else {
        thread_pool_->Enqueue(runner);
    }
}

// 显式实例化常用的模板类型
template void CoroutineManager::schedule<int>(Task<int>&&, std::shared_ptr<utils::ThreadPool>);
template void CoroutineManager::schedule<bool>(Task<bool>&&, std::shared_ptr<utils::ThreadPool>);
template void CoroutineManager::schedule<std::string>(Task<std::string>&&, std::shared_ptr<utils::ThreadPool>);
template void CoroutineManager::schedule<void>(Task<void>&&, std::shared_ptr<utils::ThreadPool>);

/**
 * @brief 包装std::future为可等待对象
 * @tparam T Future值类型
 * @param future std::future对象
 * @return FutureAwaiter<T> 可等待对象
 */
template<typename T>
FutureAwaiter<T> CoroutineManager::make_future_awaiter(std::future<T>&& future) {
    return FutureAwaiter<T>(std::move(future));
}

/**
 * @brief 创建延迟等待器
 * @param ms 延迟毫秒数
 * @return DelayAwaiter 延迟等待器
 */
DelayAwaiter CoroutineManager::make_delay_awaiter(std::chrono::milliseconds ms) {
    return DelayAwaiter(ms);
}

} // namespace im::common