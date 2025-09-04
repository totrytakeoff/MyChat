#include <iostream>
#include <chrono>
#include <thread>
#include "coroutine_manager.hpp"

using namespace im::common;

// 简单的协程函数，直接返回一个Task
Task<int> simple_task() {
    std::cout << "Simple task started and completed\n";
    co_return 42;
}

int main() {
    std::cout << "Testing Task awaitable functionality\n";
    
    // 测试直接使用get()方法
    std::cout << "Creating simple_task...\n";
    auto task1 = simple_task();
    std::cout << "Calling task1.get()...\n";
    auto result1 = task1.get();
    std::cout << "Result from get(): " << result1 << "\n\n";
    
    // 测试await_ready方法
    std::cout << "Testing await_ready...\n";
    auto task2 = simple_task();
    std::cout << "Task2 ready: " << task2.await_ready() << "\n";
    auto result2 = task2.await_resume();
    std::cout << "Result from await_resume: " << result2 << "\n\n";
    
    std::cout << "All tests completed!\n";
    return 0;
}