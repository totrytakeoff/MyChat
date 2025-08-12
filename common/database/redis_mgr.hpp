#ifndef REDIS_MGR_HPP
#define REDIS_MGR_HPP

/******************************************************************************
 *
 * @file       redis_mgr.hpp
 * @brief      Redis管理器，基于连接池管理sw::redis::Redis连接
 *
 * @author     myself
 * @date       2025/08/12
 * 
 *****************************************************************************/

#include <sw/redis++/redis++.h>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>
#include "../utils/connection_pool.hpp"
#include "../utils/config_mgr.hpp"
#include "../utils/log_manager.hpp"
#include "../utils/singleton.hpp"

namespace im::db {

/**
 * Redis配置结构
 */
struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string password = "";
    int db = 0;
    int pool_size = 10;
    int connect_timeout = 1000;  // ms
    int socket_timeout = 1000;   // ms
    
    // 从配置文件加载
    static RedisConfig from_file(const std::string& config_path);
    
    // 转换为redis-plus-plus连接选项
    sw::redis::ConnectionOptions to_connection_options() const;
    sw::redis::ConnectionPoolOptions to_pool_options() const;
};

/**
 * Redis连接池管理器
 * 直接使用sw::redis::Redis，通过连接池管理连接
 */
class RedisManager : public im::utils::Singleton<RedisManager> {
public:
    using RedisPtr = std::shared_ptr<sw::redis::Redis>;
    using RedisConnectionPool = im::utils::ConnectionPool<sw::redis::Redis>;
    
    /**
     * 初始化Redis管理器
     * @param config_path 配置文件路径
     */
    bool initialize(const std::string& config_path);
    
    /**
     * 初始化Redis管理器
     * @param config Redis配置
     */
    bool initialize(const RedisConfig& config);
    
    /**
     * 获取Redis连接（自动归还连接池）
     * @return RAII包装的Redis连接
     */
    class RedisConnection {
    public:
        explicit RedisConnection(RedisConnectionPool& pool);
        ~RedisConnection();
        
        // 禁止拷贝，允许移动
        RedisConnection(const RedisConnection&) = delete;
        RedisConnection& operator=(const RedisConnection&) = delete;
        RedisConnection(RedisConnection&&) = default;
        RedisConnection& operator=(RedisConnection&&) = default;
        
        // 获取原始Redis对象
        sw::redis::Redis* operator->() { return redis_.get(); }
        sw::redis::Redis& operator*() { return *redis_; }
        
        // 检查连接是否有效
        bool is_valid() const { return redis_ != nullptr; }
        
    private:
        RedisConnectionPool& pool_;
        RedisPtr redis_;
    };
    
    /**
     * 获取Redis连接
     */
    RedisConnection get_connection();
    
    /**
     * 便捷方法：执行Redis操作（自动管理连接）
     * @param func 要执行的操作，参数为sw::redis::Redis&
     * @return 操作结果
     */
    template<typename Func>
    auto execute(Func&& func) -> decltype(func(std::declval<sw::redis::Redis&>())) {
        auto conn = get_connection();
        if (!conn.is_valid()) {
            throw std::runtime_error("Failed to get Redis connection");
        }
        return func(*conn);
    }
    
    /**
     * 便捷方法：执行Redis操作（带异常处理）
     * @param func 要执行的操作
     * @param default_value 发生异常时的默认返回值
     * @return 操作结果或默认值
     */
    template<typename Func, typename DefaultType>
    auto safe_execute(Func&& func, DefaultType&& default_value) -> decltype(func(std::declval<sw::redis::Redis&>())) {
        try {
            return execute(std::forward<Func>(func));
        } catch (const std::exception& e) {
            im::utils::LogManager::GetLogger("redis_manager")
                ->error("Redis operation failed: {}", e.what());
            return std::forward<DefaultType>(default_value);
        }
    }
    
    /**
     * 获取连接池统计信息
     */
    struct PoolStats {
        size_t total_connections;
        size_t available_connections;
        size_t active_connections;
    };
    PoolStats get_pool_stats() const;
    
    /**
     * 检查连接池健康状态
     */
    bool is_healthy() const;
    
    /**
     * 关闭管理器
     */
    void shutdown();

private:
    friend class im::utils::Singleton<RedisManager>;
    RedisManager() = default;
    ~RedisManager() = default;
    
    RedisConfig config_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
    
    // 创建Redis连接的工厂函数
    RedisPtr create_redis_connection();
};

// 便捷的全局访问函数
inline RedisManager& redis_manager() {
    return RedisManager::GetInstance();
}

} // namespace im::db

#endif // REDIS_MGR_HPP