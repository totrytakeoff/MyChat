#include <iostream>
#include "../../common/database/redis_mgr.hpp"

int main() {
    try {
        std::cout << "=== Setting up loggers ===" << std::endl;
        im::utils::LogManager::SetLogToConsole("redis_manager");
        im::utils::LogManager::SetLogToConsole("connection_pool");
        
        std::cout << "=== Creating config ===" << std::endl;
        im::db::RedisConfig config;
        config.host = "127.0.0.1";
        config.port = 6379;
        config.password = "myself";
        config.db = 1;
        config.pool_size = 2;
        config.connect_timeout = 2000;
        config.socket_timeout = 2000;
        
        std::cout << "=== Getting manager instance ===" << std::endl;
        auto& mgr = im::db::redis_manager();
        
        std::cout << "=== Starting initialization ===" << std::endl;
        bool result = mgr.initialize(config);
        
        std::cout << "=== Initialization result: " << (result ? "SUCCESS" : "FAILURE") << " ===" << std::endl;
        
        if (result) {
            std::cout << "=== Testing health check ===" << std::endl;
            bool healthy = mgr.is_healthy();
            std::cout << "Health check: " << (healthy ? "HEALTHY" : "UNHEALTHY") << std::endl;
            
            if (healthy) {
                std::cout << "=== Getting pool stats ===" << std::endl;
                auto stats = mgr.get_pool_stats();
                std::cout << "Pool stats - Total: " << stats.total_connections 
                         << ", Available: " << stats.available_connections
                         << ", Active: " << stats.active_connections << std::endl;
            }
        }
        
        std::cout << "=== Test completed ===" << std::endl;
        return result ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
}