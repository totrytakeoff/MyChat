#ifndef CONNECTION_POOL_HPP
#define CONNECTION_POOL_HPP


/******************************************************************************
 *
 * @file       connection_pool.hpp
 * @brief      通用连接池类
 *
 * @author     myself
 * @date       2025/07/14
 *
 *****************************************************************************/

#include <spdlog/spdlog.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include "singleton.hpp"
#include "log_manager.hpp"


template <typename T>
class ConnectionPool : public Singleton<ConnectionPool<T>> {
    friend class Singleton<ConnectionPool<T>>;

public:
    // 工厂方法，用于创建连接池实例
    using ConnectionPtr = std::shared_ptr<T>;
    using ConnectionFactory = std::function<ConnectionPtr()>;

    ~ConnectionPool();

    /**
     * @brief      初始化连接池
     *
     * @param[in]   poolSize 连接池大小
     * @param[in]   factory   连接工厂函数，用于创建连接实例
     * @return     void
     */
    void Init(size_t poolSize, ConnectionFactory factory);

    /**
     * @brief      获取连接
     *
     * @return     std::shared_ptr<T> 返回连接实例
     */
    ConnectionPtr GetConnection();

    /**
     * @brief      释放连接
     *
     * @param[in]  conn 连接实例
     * @return     void
     */
    void ReleaseConnection(const ConnectionPtr& conn);

    /**
     * @brief      关闭连接池，释放所有连接
     *
     * @return     void
     */
    void Close();

    size_t GetPoolSize() const { return  m_poolSize_; }

    size_t GetAvailableCount() const;

    size_t GetInUsedCount() const;

private:
    ConnectionPool() ;
    // 禁止拷贝构造和赋值，防止多实例
    ConnectionPool(const ConnectionPool<T>&) = delete;
    ConnectionPool& operator=(const ConnectionPool<T>&) = delete;

    // 连接池大小
    size_t m_poolSize_ = 0;
    // 连接工厂函数
    ConnectionFactory m_factory;
    // 连接队列
    std::queue<ConnectionPtr> m_connections;
    // 互斥锁，保护连接队列
    mutable std::mutex m_mutex; // 使const方法可加锁
    // 条件变量，用于通知获取连接的线程
    std::condition_variable m_condVar;
    // 是否已关闭标志
    std::atomic<bool> m_isClosed{false};
};



template <typename T>
ConnectionPool<T>::ConnectionPool() : m_poolSize_(0), m_isClosed(false) {}


template <typename T>
ConnectionPool<T>::~ConnectionPool() {
    Close();
}


template <typename T>
void ConnectionPool<T>::Init(size_t poolSize, ConnectionFactory factory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_poolSize_ > 0) {
        if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->warn("Connection pool already initialized.");
        return;
    }
    m_poolSize_ = poolSize;
    m_factory = std::move(factory);
    m_isClosed = false;

    try {
        for (size_t i = 0; i < m_poolSize_; ++i) {
            auto conn = m_factory();
            if (conn) {
                m_connections.push(conn);
            }
        }
    } catch (const std::exception& e) {
        if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->error("Failed to initialize connection pool: {}", e.what());
        throw;
    }

    if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->info("Connection pool initialized with size: {}", m_poolSize_);
}

template <typename T>
typename ConnectionPool<T>::ConnectionPtr ConnectionPool<T>::GetConnection() {
    std::unique_lock<std::mutex> lock(m_mutex);

    m_condVar.wait(lock, [this]() { return m_isClosed || !m_connections.empty(); });

    if (m_isClosed) {
        if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->error("Connection pool is closed, cannot get connection.");
        return nullptr;
    }

    if (m_connections.empty()) {
        // 如果连接池为空，尝试创建新的连接
        if (m_factory) {
            return m_factory();
        }

        if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->error("No available connections in the pool.");
        return nullptr;
    }


    // 从连接池中获取一个连接
    auto conn = m_connections.front();
    m_connections.pop();

    if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->info("Connection acquired from pool, remaining connections: {}", m_connections.size());

    return conn;

}


template <typename T>
void ConnectionPool<T>::ReleaseConnection(const ConnectionPtr& conn) {
    if (!conn) {
        if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->error("Attempted to release a null connection.");
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isClosed) {
        if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->warn("Connection pool is closed, cannot release connection.");
        return;
    }

    if (m_connections.size() >= m_poolSize_) {
        if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->warn("Connection pool is full, discarding connection.");
        return;  // 如果连接池已满，直接丢弃连接
    }

    m_connections.push(conn);
    m_condVar.notify_one();  // 通知等待获取连接的线程

    if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->debug("Connection released back to pool, total connections: {}", m_connections.size());
}


template <typename T>
void ConnectionPool<T>::Close() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_isClosed = true;
    while (!m_connections.empty()) {
        m_connections.pop();
    }
    m_factory = nullptr; // 清空工厂函数
    m_condVar.notify_all();  // 通知所有等待的线程
    if (LogManager::IsLoggingEnabled("connection_pool")) LogManager::GetLogger("connection_pool")->info("Connection pool closed, all connections released.");
}

template <typename T>
size_t ConnectionPool<T>::GetAvailableCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connections.size();
}


template <typename T>
size_t ConnectionPool<T>::GetInUsedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_poolSize_ - m_connections.size();
}

#endif  // CONNECTION_POOL_HPP

