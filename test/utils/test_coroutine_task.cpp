#include <iostream>
#include <chrono>
#include <thread>
#include "../../common/utils/coroutine_manager.hpp"

using namespace im::common;

// 简单的协程函数，直接返回一个Task
Task<int> simple_task() {
    std::cout << "Simple task started\n";
    std::cout << "Simple task completed\n";
    co_return 42;
}

// 使用co_await的协程函数
Task<int> await_task() {
    std::cout << "Await task started\n";
    auto result = co_await simple_task();
    std::cout << "Await task got result: " << result << "\n";
    co_return result * 2;
}

int main() {
    std::cout << "Testing Task awaitable functionality\n";
    
    // 获取协程管理器实例
    auto& coroutine_manager = CoroutineManager::getInstance();
    
    // 测试直接使用get()方法
    std::cout << "Creating and scheduling simple_task...\n";
    auto task1 = simple_task();
    coroutine_manager.schedule(std::move(task1));
    // 等待一段时间确保协程执行完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Simple task scheduled\n\n";
    
    // 测试使用co_await（我们需要调度协程）
    std::cout << "Creating and scheduling await_task...\n";
    auto task2 = await_task();
    coroutine_manager.schedule(std::move(task2));
    // 等待一段时间确保协程执行完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Await task scheduled\n";
    
    return 0;
}