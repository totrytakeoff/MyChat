#include <iostream>
#include "../../common/database/redis_mgr.hpp"

int main() {
    try {
        std::cout << "Setting up logger..." << std::endl;
        im::utils::LogManager::SetLogToConsole("redis_manager");
        im::utils::LogManager::SetLogToConsole("connection_pool");
        
        std::cout << "Creating Redis config..." << std::endl;
        im::db::RedisConfig config;
        config.host = "127.0.0.1";
        config.port = 6379;
        config.password = "myself";
        config.db = 1;
        config.pool_size = 2;
        config.connect_timeout = 2000;
        config.socket_timeout = 2000;
        
        std::cout << "Getting Redis manager instance..." << std::endl;
        auto& mgr = im::db::redis_manager();
        
        std::cout << "Initializing Redis manager..." << std::endl;
        bool result = mgr.initialize(config);
        
        std::cout << "Initialize result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
        
        if (result) {
            std::cout << "Testing connection..." << std::endl;
            auto conn = mgr.get_connection();
            if (conn.is_valid()) {
                std::cout << "Connection is valid, testing ping..." << std::endl;
                conn->ping();
                std::cout << "Ping successful!" << std::endl;
            }
        }
        
        return result ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}