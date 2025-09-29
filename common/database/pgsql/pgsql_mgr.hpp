#ifndef PGSQL_MGR_HPP
#define PGSQL_MGR_HPP

/******************************************************************************
 *
 * @file       pgsql_mgr.hpp
 * @brief      PgSqlManager 管理 PgSqlConnection 连接池
 *
 * @author     myself
 * @date       2025/09/28
 *
 *****************************************************************************/


#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "../../utils/config_mgr.hpp"
#include "../../utils/connection_pool.hpp"
#include "../../utils/log_manager.hpp"
#include "../../utils/singleton.hpp"
#include "pgsql_conn.hpp"


namespace im::db {

using utils::ConfigManager;
using utils::ConnectionPool;
using utils::LogManager;
using utils::Singleton;

class PgSqlManager : public Singleton<PgSqlManager> {
public:
    using PgSqlConnectionPtr = std::shared_ptr<PgSqlConnection>;
    using PgSqlConnectionPool = ConnectionPool<PgSqlConnection>;

    class PgSqlConnectionWrapper {
    public:
        explicit PgSqlConnectionWrapper(PgSqlConnectionPool& pool);
        ~PgSqlConnectionWrapper();
        // 禁止拷贝，允许移动
        PgSqlConnectionWrapper(const PgSqlConnectionWrapper&) = delete;
        PgSqlConnectionWrapper& operator=(const PgSqlConnectionWrapper&) = delete;
        PgSqlConnectionWrapper(PgSqlConnectionWrapper&&) = default;
        PgSqlConnectionWrapper& operator=(PgSqlConnectionWrapper&&) = default;

        // 获取原始连接对象
        PgSqlConnection* operator->() { return connection_.get(); }
        PgSqlConnection& operator*() {
            if (!connection_) {
                throw std::runtime_error("连接不可用");
            }
            return *connection_;
        }
        bool is_valid() const { return connection_ != nullptr && connection_->is_valid(); }

    private:
        PgSqlConnectionPool& pool_;
        PgSqlConnectionPtr connection_;
    };

    PgSqlConnectionWrapper get_connection();


    bool initialize(const std::string& config_path);

    bool initialize(const PgSqlConfig& config);



    template <typename Func>
    auto execute(Func&& func) -> decltype(func(std::declval<PgSqlConnection&>()));

    template <typename Func>
    auto execute_transaction(Func&& func) -> decltype(func(std::declval<PgSqlConnection&>()));


    template <typename Func, typename DefaultType>
    auto safe_execute(Func&& func, DefaultType&& default_value)
            -> decltype(func(std::declval<PgSqlConnection&>()));



    template <typename Func, typename DefaultType>
    auto safe_execute_transaction(Func&& func, DefaultType&& default_value)
            -> decltype(func(std::declval<PgSqlConnection&>()));



    // ========== 管理和监控 ==========

    /**
     * 连接池统计信息
     */
    struct PoolStats {
        size_t total_connections;                     // 总连接数
        size_t available_connections;                 // 可用连接数
        size_t active_connections;                    // 使用中的连接数
        double success_rate;                          // 成功率
        size_t total_operations;                      // 总操作数
        size_t failed_operations;                     // 失败操作数
        std::chrono::milliseconds avg_response_time;  // 平均响应时间
    };


    // 获取连接池统计信息
    PoolStats get_pool_stats() const;


    bool is_healthy() const;

    // 初始化数据库模式
    bool init_schema(bool drop_existing = false);


    // 重新加载配置,重新初始化连接池
    bool reload_config(const std::string& config_path);
    bool reload_config(const PgSqlConfig& config);

    void shutdown();

private:
    friend class im::utils::Singleton<PgSqlManager>;
    PgSqlManager() = default;
    ~PgSqlManager() { shutdown(); }

    bool init_connection_pool(const PgSqlConfig& config);


    void shutdown_connection_pool();

    // 创建数据库连接工厂函数
    PgSqlConnectionPtr create_connection();

    // ========== 统计和监控 ==========

    void record_operation_success(const std::chrono::steady_clock::time_point& start_time) {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        operation_count_.fetch_add(1);
        total_response_time_.fetch_add(duration_ms.count());
    }

    void record_operation_failure(const std::chrono::steady_clock::time_point& start_time,
                                  const std::string& error) {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        operation_count_.fetch_add(1);
        failure_count_.fetch_add(1);
        total_response_time_.fetch_add(duration_ms.count());

        if (auto logger = im::utils::LogManager::GetLogger("pgsql_manager")) {
            logger->warn("数据库操作失败，耗时: {}ms, 错误: {}", duration_ms.count(), error);
        }
    }

    void record_transaction_success(const std::chrono::steady_clock::time_point& start_time) {
        record_operation_success(start_time);

        if (auto logger = im::utils::LogManager::GetLogger("pgsql_manager")) {
            auto duration = std::chrono::steady_clock::now() - start_time;
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            logger->debug("事务执行成功，耗时: {}ms", duration_ms.count());
        }
    }

    void record_transaction_failure(const std::chrono::steady_clock::time_point& start_time,
                                    const std::string& error) {
        record_operation_failure(start_time, error);

        if (auto logger = im::utils::LogManager::GetLogger("pgsql_manager")) {
            logger->error("事务执行失败: {}", error);
        }
    }

    void record_safe_operation_failure(const std::string& error) {
        failure_count_.fetch_add(1);

        if (auto logger = im::utils::LogManager::GetLogger("pgsql_manager")) {
            logger->warn("安全操作失败: {}", error);
        }
    }

    void record_safe_transaction_failure(const std::string& error) {
        failure_count_.fetch_add(1);

        if (auto logger = im::utils::LogManager::GetLogger("pgsql_manager")) {
            logger->warn("安全事务操作失败: {}", error);
        }
    }

private:
    // ========== 数据成员 ==========
    PgSqlConfig config_;        // 配置信息
    mutable std::mutex mutex_;  // 线程安全锁
    bool initialized_ = false;  // 初始化状态

    // 统计信息（原子操作，线程安全）
    std::atomic<size_t> operation_count_{0};      // 操作计数
    std::atomic<size_t> failure_count_{0};        // 失败计数
    std::atomic<size_t> total_response_time_{0};  // 总响应时间（毫秒）
};

// ========== 便捷访问函数 ==========

/**
 * 全局访问函数
 *
 * 提供简洁的访问方式
 */
inline PgSqlManager& pgsql_manager() { return PgSqlManager::GetInstance(); }

/**
 * 快速执行函数
 *
 * 最简洁的使用方式
 */
template <typename Func>
auto pgsql_execute(Func&& func) -> decltype(func(std::declval<PgSqlConnection&>())) {
    return pgsql_manager().execute(std::forward<Func>(func));
}

/**
 * 快速事务执行函数
 */
template <typename Func>
auto pgsql_transaction(Func&& func) -> decltype(func(std::declval<PgSqlConnection&>())) {
    return pgsql_manager().execute_transaction(std::forward<Func>(func));
}



}  // namespace im::db



#endif  // PGSQL_MGR_HPP
