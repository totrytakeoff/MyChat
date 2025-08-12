#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>
#include "../../common/utils/connection_pool.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../../common/utils/thread_pool.hpp"

using namespace im::utils;


// 用于连接池测试的假连接类型
typedef struct DummyConn {
    int id;
    DummyConn(int i) : id(i) {}
} DummyConn;

// 连接工厂
std::shared_ptr<DummyConn> DummyConnFactory() {
    static std::atomic<int> counter{0};
    return std::make_shared<DummyConn>(++counter);
}

TEST(ThreadPoolTest, BasicTaskExecution) {
    auto& pool = ThreadPool::GetInstance();
    pool.Init(4);
    std::atomic<int> result{0};
    auto fut = pool.Enqueue([&result] { result = 42; });
    fut.get();
    EXPECT_EQ(result, 42);
    pool.Shutdown();
}

TEST(ConnectionPoolTest, BasicConnectionAcquireRelease) {
    using Pool = ConnectionPool<DummyConn>;
    auto& pool = Pool::GetInstance();
    pool.Init(2, DummyConnFactory);
    auto conn1 = pool.GetConnection();
    auto conn2 = pool.GetConnection();
    EXPECT_NE(conn1, nullptr);
    EXPECT_NE(conn2, nullptr);
    EXPECT_EQ(pool.GetAvailableCount(), 0);
    pool.ReleaseConnection(conn1);
    EXPECT_EQ(pool.GetAvailableCount(), 1);
    pool.Close();
}

TEST(LogManagerTest, LogToFileAndSwitch) {
    const std::string logger = "test_logger";
    const std::string filename = "test_log.txt";
    LogManager::SetLogToFile(logger, filename);
    LogManager::GetLogger(logger)->info("hello file log");
    LogManager::SetLoggingEnabled(logger, false);
    LogManager::GetLogger(logger)->info("should not appear");
    LogManager::SetLoggingEnabled(logger, true);
    LogManager::SetLogToConsole(logger);
    LogManager::GetLogger(logger)->info("hello console log");
    // 检查文件内容
    std::ifstream fin(filename);
    std::string line;
    bool found = false;
    while (std::getline(fin, line)) {
        if (line.find("hello file log") != std::string::npos) {
            found = true;
            break;
        }
    }
    fin.close();
    EXPECT_TRUE(found);
}

TEST(LogManagerTest, enableORdisable_Log) {
    LogManager::SetLogToConsole("test");
    LogManager::GetLogger("test")->info("This is a test log message --- {}", "enable1");
    if (LogManager::IsLoggingEnabled("test")) {
        LogManager::GetLogger("test")->info("This is a test log message --- {}", "enable2");
    }

    LogManager::SetLoggingEnabled("test", false);
    LogManager::GetLogger("test")->info("This log should not appear 1");
    if (LogManager::IsLoggingEnabled("test")) {
        LogManager::GetLogger("test")->info("This log should not appear 2");
    }

    LogManager::SetLoggingEnabled("test", true);
    LogManager::GetLogger("test")->info("This is a test log message --- {}", "enable1");
    if (LogManager::IsLoggingEnabled("test")) {
        LogManager::GetLogger("test")->info("This is a test log message --- {}", "enable2");
    }
}
