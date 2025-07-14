#include "spdlog/spdlog.h"
#include "thread_pool.hpp"
#include "log_manager.hpp"

ThreadPool::ThreadPool() : m_shutdown(false), m_tasksCount(0) {}

ThreadPool::~ThreadPool() { Shutdown(); }

void ThreadPool::Init(size_t threadCount) {
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
    }

    m_shutdown = false;
    m_tasksCount = 0;

    for (size_t i = 0; i < threadCount; ++i) {
        m_threads.emplace_back(&ThreadPool::WorkerThread, this);
    }
    if (LogManager::IsLoggingEnabled("thread_pool"))
        LogManager::GetLogger("thread_pool")->info("ThreadPool initialized with {} threads", threadCount);
}

void ThreadPool::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown = true;
    }

    m_condition.notify_all();

    // 等待所有线程完成
    for (std::thread& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    m_threads.clear();  // 清空线程池
    if (LogManager::IsLoggingEnabled("thread_pool"))
        LogManager::GetLogger("thread_pool")->info("ThreadPool shutdown completed");
}

void ThreadPool::WorkerThread() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this] { return m_shutdown || !m_tasks.empty(); });

            if (m_shutdown && m_tasks.empty()) {
                return;  // 线程池关闭且没有任务时退出
            }

            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
        }

        if (task) {
            try {
                task();  // 执行任务
            } catch (const std::exception& e) {
                if (LogManager::IsLoggingEnabled("thread_pool"))
                    LogManager::GetLogger("thread_pool")->error("Exception in thread pool worker: {}", e.what());
            } catch (...) {
                if (LogManager::IsLoggingEnabled("thread_pool"))
                    LogManager::GetLogger("thread_pool")->error("Unknown exception in thread pool worker");
            }
            m_tasksCount--;  // 任务完成后减少任务计数
        }
    }
}

size_t ThreadPool::GetTaskCount() const { return m_tasksCount.load(); }
