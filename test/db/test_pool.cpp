#include <iostream>
#include <sw/redis++/redis++.h>
#include "../../common/utils/connection_pool.hpp"
#include "../../common/utils/log_manager.hpp"

using RedisPtr = std::shared_ptr<sw::redis::Redis>;
using RedisConnectionPool = im::utils::ConnectionPool<sw::redis::Redis>;

RedisPtr create_redis_connection() {
    try {
        std::cout << "Creating Redis connection..." << std::endl;
        
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.password = "myself";
        opts.db = 1;
        opts.connect_timeout = std::chrono::milliseconds(2000);
        opts.socket_timeout = std::chrono::milliseconds(2000);
        
        auto redis = std::make_shared<sw::redis::Redis>(opts);
        std::cout << "Redis connection created successfully" << std::endl;
        
        return redis;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create Redis connection: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    try {
        // 初始化日志
        im::utils::LogManager::SetLogToConsole("connection_pool");
        
        std::cout << "Getting connection pool instance..." << std::endl;
        auto& pool = RedisConnectionPool::GetInstance();
        std::cout << "Got connection pool instance" << std::endl;
        
        std::cout << "Initializing connection pool with size 2..." << std::endl;
        pool.Init(2, create_redis_connection);
        std::cout << "Connection pool initialized" << std::endl;
        
        std::cout << "Getting connection from pool..." << std::endl;
        auto conn = pool.GetConnection();
        std::cout << "Got connection from pool" << std::endl;
        
        if (conn) {
            std::cout << "Testing connection with ping..." << std::endl;
            conn->ping();
            std::cout << "Ping successful" << std::endl;
        }
        
        std::cout << "Releasing connection..." << std::endl;
        pool.ReleaseConnection(conn);
        std::cout << "Connection released" << std::endl;
        
        std::cout << "All tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}