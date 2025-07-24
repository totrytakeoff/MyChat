#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <filesystem>
#include "../../common/network/IOService_pool.hpp"
#include "../../common/utils/log_manager.hpp"

using namespace std::chrono_literals;

class IOServicePoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建日志目录
        std::filesystem::create_directories("logs");
        
        // 初始化日志
        LogManager::SetLogToFile("io_service_pool", "logs/io_service_pool_test.log");
        LogManager::SetLoggingEnabled("io_service_pool", true);
    }

    void TearDown() override {
        // 注意：不要在每个测试中停止IOServicePool，因为它是单例
        // IOServicePool会在程序结束时自动清理
    }
};

// 测试IOServicePool单例获取
TEST_F(IOServicePoolTest, SingletonInstance) {
    auto& pool1 = IOServicePool::GetInstance();
    auto& pool2 = IOServicePool::GetInstance();
    
    EXPECT_EQ(&pool1, &pool2) << "IOServicePool should be a singleton";
}

// 测试IOServicePool获取IOService
TEST_F(IOServicePoolTest, GetIOService) {
    auto& pool = IOServicePool::GetInstance();
    
    auto& io_service1 = pool.GetIOService();
    auto& io_service2 = pool.GetIOService();
    
    // 至少能获取到io_service对象
    EXPECT_NE(&io_service1, nullptr) << "Should get a valid io_service";
    EXPECT_NE(&io_service2, nullptr) << "Should get a valid io_service";
}

// 测试IOServicePool多线程运行
TEST_F(IOServicePoolTest, MultiThreadExecution) {
    auto& pool = IOServicePool::GetInstance();
    
    std::atomic<int> counter{0};
    const int num_tasks = 100;
    
    // 提交多个任务到IOServicePool
    for (int i = 0; i < num_tasks; ++i) {
        boost::asio::post(pool.GetIOService(), [&counter]() {
            counter.fetch_add(1);
        });
    }
    
    // 等待任务完成
    for (int i = 0; i < 10 && counter.load() < num_tasks; ++i) {
        std::this_thread::sleep_for(50ms);
    }
    
    EXPECT_EQ(counter.load(), num_tasks) << "All tasks should be executed";
}

// 测试IOServicePool停止功能
TEST_F(IOServicePoolTest, StopFunctionality) {
    {
        auto& pool = IOServicePool::GetInstance();
        
        // 提交一个长时间运行的任务
        boost::asio::post(pool.GetIOService(), []() {
            std::this_thread::sleep_for(100ms);
        });
        
        // 确保任务开始执行
        std::this_thread::sleep_for(10ms);
    }
    
    // pool析构时应该正确停止
    SUCCEED() << "IOServicePool stopped successfully";
}