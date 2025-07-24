#include <iostream>
#include "../utils/log_manager.hpp"
#include "IOService_pool.hpp"


IOServicePool::IOServicePool(std::size_t pool_size)
        : pool_size_(pool_size), next_io_service_(0) {
    if (pool_size_ == 0) {
        pool_size_ = std::thread::hardware_concurrency();
        if (pool_size_ == 0) {
            pool_size_ = 1; // 至少创建一个线程
        }
    }
    
    works_.reserve(pool_size_);
    io_services_.reserve(pool_size_);
    for (std::size_t i = 0; i < pool_size_; ++i) {
        io_services_.emplace_back(std::make_shared<IOService>());
        works_.emplace_back(std::make_unique<Work>(boost::asio::make_work_guard(*io_services_[i])));
        threads_.emplace_back([io_service = io_services_[i]]() {
            if (LogManager::IsLoggingEnabled("io_service_pool")) {
                LogManager::GetLogger("io_service_pool")
                        ->info("IO service thread started: {}", 
                               std::hash<std::thread::id>{}(std::this_thread::get_id()));
            }
            io_service->run();
            if (LogManager::IsLoggingEnabled("io_service_pool")) {
                LogManager::GetLogger("io_service_pool")
                        ->info("IO service thread ended: {}", 
                               std::hash<std::thread::id>{}(std::this_thread::get_id()));
            }
        });
    }
}


IOServicePool::~IOServicePool() {
    Stop();
    std::cout << "IOServicePool destroyed." << std::endl;
}

/**
 * @brief      从IOService池中获取一个IOService实例
 *
 * @param[in]  参数名  参数描述
 * @return     返回一个IOService实例的引用
 */

boost::asio::io_context& IOServicePool::GetIOService() {
    // 确保pool_size_不为0，防止除零错误
    if (pool_size_ == 0) {
        throw std::runtime_error("IOServicePool has been stopped or not initialized properly");
    }
    
    // 使用atomic变量保证线程安全
    size_t index = next_io_service_.fetch_add(1, std::memory_order_relaxed) % pool_size_;
    return *io_services_[index];
}

void IOServicePool::Stop() {
    if (pool_size_ == 0) return; // 防止重复停止
    
    // 因为仅仅执行work.reset并不能让iocontext从run的状态中退出
    // 当iocontext已经绑定了读或写的监听事件后，还需要手动stop该服务。

    for (auto& work : works_) {
        work.reset();
    }
    works_.clear(); // 清空work指针容器

    for (auto& service : io_services_) {
        // 把服务停止
        service->stop();
    }

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear(); // 清空线程容器
    io_services_.clear(); // 清空io_service容器
    
    pool_size_ = 0; // 标记已停止
}
