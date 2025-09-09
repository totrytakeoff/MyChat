#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>

// 包含基础组件
#include "../common/utils/service_identity.hpp"
#include "../common/utils/log_manager.hpp"
#include "../common/utils/thread_pool.hpp"
#include "../common/utils/coroutine_manager.hpp"

using namespace im::utils;
using namespace im::common;

// 测试基础组件功能
class BasicComponentsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置环境变量
        setenv("SERVICE_NAME", "test_gateway", 1);
        setenv("CLUSTER_ID", "test-cluster", 1);
        setenv("REGION", "test-region", 1);
        setenv("INSTANCE_ID", "test-instance-001", 1);
    }
};

// 测试服务标识管理器
TEST_F(BasicComponentsTest, ServiceIdentityManager) {
    auto& identity_mgr = ServiceIdentityManager::getInstance();
    
    // 测试初始化
    EXPECT_TRUE(identity_mgr.initializeFromEnv("test_gateway"));
    EXPECT_TRUE(identity_mgr.isInitialized());
    
    // 测试获取信息
    const auto& identity = identity_mgr.getServiceIdentity();
    EXPECT_EQ(identity.service_name, "test_gateway");
    EXPECT_EQ(identity.cluster_id, "test-cluster");
    EXPECT_EQ(identity.region, "test-region");
    
    // 测试设备ID和平台信息
    std::string device_id = identity_mgr.getDeviceId();
    std::string platform_info = identity_mgr.getPlatformInfo();
    
    EXPECT_FALSE(device_id.empty());
    EXPECT_FALSE(platform_info.empty());
    EXPECT_TRUE(device_id.find("test_gateway") != std::string::npos);
    
    std::cout << "Device ID: " << device_id << std::endl;
    std::cout << "Platform Info: " << platform_info << std::endl;
}

// 测试线程池
TEST_F(BasicComponentsTest, ThreadPool) {
    auto& thread_pool = ThreadPool::GetInstance();
    
    // 初始化线程池
    thread_pool.Init(4);
    
    // 测试任务提交
    std::atomic<int> counter(0);
    const int num_tasks = 100;
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < num_tasks; ++i) {
        auto future = thread_pool.Enqueue([&counter]() {
            counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        futures.push_back(std::move(future));
    }
    
    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), num_tasks);
    
    thread_pool.Shutdown();
}

// 测试协程管理器
TEST_F(BasicComponentsTest, CoroutineManager) {
    // 重新初始化线程池以确保它可用
    auto& thread_pool = ThreadPool::GetInstance();
    thread_pool.Init(2);
    
    auto& coro_mgr = CoroutineManager::getInstance();
    
    std::atomic<bool> task_completed(false);
    
    // 创建一个简单的协程任务
    auto simple_task = []() -> Task<int> {
        co_await DelayAwaiter(std::chrono::milliseconds(100));
        co_return 42;
    };
    
    // 调度协程
    coro_mgr.schedule([&task_completed, task = simple_task()]() mutable -> Task<void> {
        auto result = co_await task;
        EXPECT_EQ(result, 42);
        task_completed = true;
    }());
    
    // 等待协程完成
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!task_completed.load() && std::chrono::steady_clock::now() < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(task_completed.load());
    
    // 测试完成后不shutdown线程池，让其他测试也能使用
}

// 测试日志管理器
TEST_F(BasicComponentsTest, LogManager) {
    // 测试日志初始化
    LogManager::SetLogToFile("test_logger", "test_logs/test.log");
    
    // 测试获取日志器
    auto logger = LogManager::GetLogger("test_logger");
    EXPECT_NE(logger, nullptr);
    
    // 测试基本日志功能
    logger->info("This is a test log message");
    logger->warn("This is a warning message");
    logger->error("This is an error message");
    
    // 日志应该能正常写入而不报错
    SUCCEED();
}

// 性能测试
TEST_F(BasicComponentsTest, PerformanceTest) {
    const int num_operations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    // 测试服务标识获取性能
    auto& identity_mgr = ServiceIdentityManager::getInstance();
    identity_mgr.initializeFromEnv("perf_test");
    
    for (int i = 0; i < num_operations; ++i) {
        auto device_id = identity_mgr.getDeviceId();
        auto platform = identity_mgr.getPlatformInfo();
        EXPECT_FALSE(device_id.empty());
        EXPECT_FALSE(platform.empty());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Performance: " << num_operations << " operations in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average: " << (double)duration.count() / num_operations 
              << " microseconds per operation" << std::endl;
    
    // 期望平均操作时间小于100微秒
    EXPECT_LT((double)duration.count() / num_operations, 100.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}