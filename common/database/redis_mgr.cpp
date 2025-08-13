#include "redis_mgr.hpp"
#include <chrono>

namespace im::db {

// ===== RedisConfig 实现 =====
RedisConfig RedisConfig::from_file(const std::string& config_path) {
    RedisConfig config;
    try {
        auto cfg_mgr = im::utils::ConfigManager(config_path);
        
        config.host = cfg_mgr.get<std::string>("redis.host", "127.0.0.1");
        config.port = cfg_mgr.get<int>("redis.port", 6379);
        config.password = cfg_mgr.get<std::string>("redis.password", "");
        config.db = cfg_mgr.get<int>("redis.db", 0);
        config.pool_size = cfg_mgr.get<int>("redis.pool_size", 10);
        config.connect_timeout = cfg_mgr.get<int>("redis.connect_timeout", 1000);
        config.socket_timeout = cfg_mgr.get<int>("redis.socket_timeout", 1000);
        
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("redis_manager")
            ->error("Failed to load Redis config: {}", e.what());
        throw;
    }
    return config;
}

sw::redis::ConnectionOptions RedisConfig::to_connection_options() const {
    sw::redis::ConnectionOptions opts;
    opts.host = host;
    opts.port = port;
    opts.password = password;
    opts.db = db;
    opts.connect_timeout = std::chrono::milliseconds(connect_timeout);
    opts.socket_timeout = std::chrono::milliseconds(socket_timeout);
    return opts;
}

sw::redis::ConnectionPoolOptions RedisConfig::to_pool_options() const {
    sw::redis::ConnectionPoolOptions opts;
    opts.size = pool_size;
    return opts;
}

// ===== RedisManager::RedisConnection 实现 =====
RedisManager::RedisConnection::RedisConnection(RedisConnectionPool& pool) 
    : pool_(pool), redis_(pool.GetConnection()) {
}

RedisManager::RedisConnection::~RedisConnection() {
    // RAII：析构时自动归还连接
    if (redis_) {
        pool_.ReleaseConnection(redis_);
    }
}

// ===== RedisManager 实现 =====
bool RedisManager::initialize(const std::string& config_path) {
    try {
        auto config = RedisConfig::from_file(config_path);
        return initialize(config);
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("redis_manager")
            ->error("Failed to initialize Redis manager with config file: {}", e.what());
        return false;
    }
}

bool RedisManager::initialize(const RedisConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        im::utils::LogManager::GetLogger("redis_manager")
            ->warn("Redis manager already initialized");
        return true;
    }
    
    try {
        config_ = config;
  
        // 初始化连接池（使用单例）
        auto& pool = RedisConnectionPool::GetInstance();
        pool.Init(config.pool_size, [this]() { return create_redis_connection(); });
        
        // 测试连接
        im::utils::LogManager::GetLogger("redis_manager")->info("Getting test connection...");
        auto test_conn = get_connection();
        im::utils::LogManager::GetLogger("redis_manager")->info("Got test connection, checking validity...");
        if (!test_conn.is_valid()) {
            throw std::runtime_error("Failed to create test connection");
        }
        
        // 执行ping测试
        im::utils::LogManager::GetLogger("redis_manager")->info("Executing ping test...");
        test_conn->ping();
        im::utils::LogManager::GetLogger("redis_manager")->info("Ping test successful");
        
        initialized_ = true;
        
        im::utils::LogManager::GetLogger("redis_manager")
            ->info("Redis manager initialized successfully. Pool size: {}, Host: {}:{}", 
                   config.pool_size, config.host, config.port);
        
        return true;
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("redis_manager")
            ->error("Failed to initialize Redis manager: {}", e.what());
        return false;
    }
}

RedisManager::RedisConnection RedisManager::get_connection() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("Redis manager not initialized");
    }
    
    auto& pool = RedisConnectionPool::GetInstance();
    return RedisConnection(pool);
}

RedisManager::PoolStats RedisManager::get_pool_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return {0, 0, 0};
    }
    
    auto& pool = RedisConnectionPool::GetInstance();
    return {
        pool.GetPoolSize(),
        pool.GetAvailableCount(),
        pool.GetInUsedCount()
    };
}

bool RedisManager::is_healthy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        // 尝试获取连接并执行ping
        auto conn = const_cast<RedisManager*>(this)->get_connection();
        if (!conn.is_valid()) {
            return false;
        }
        
        conn->ping();
        return true;
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("redis_manager")
            ->error("Health check failed: {}", e.what());
        return false;
    }
}

void RedisManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    auto& pool = RedisConnectionPool::GetInstance();
    pool.Close();
    initialized_ = false;
    
    im::utils::LogManager::GetLogger("redis_manager")
        ->info("Redis manager shutdown");
}

RedisManager::RedisPtr RedisManager::create_redis_connection() {
    try {
        auto logger = im::utils::LogManager::GetLogger("redis_manager");
        logger->info("Creating Redis connection to {}:{}", config_.host, config_.port);
        
        auto conn_opts = config_.to_connection_options();
        
        // 创建Redis连接（注意：不使用内部连接池，因为我们自己管理连接池）
        logger->info("Attempting to connect to Redis...");
        auto redis = std::make_shared<sw::redis::Redis>(conn_opts);
        logger->info("Redis connection created successfully");
        
        return redis;
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("redis_manager")
            ->error("Failed to create Redis connection: {}", e.what());
        throw;
    }
}

} // namespace im::db