#include <iostream>
#include <sw/redis++/redis++.h>
#include <chrono>

int main() {
    try {
        std::cout << "Testing Redis connection..." << std::endl;
        
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.password = "myself";
        opts.db = 1;
        opts.connect_timeout = std::chrono::milliseconds(2000);
        opts.socket_timeout = std::chrono::milliseconds(2000);
        
        std::cout << "Creating Redis connection..." << std::endl;
        auto redis = std::make_shared<sw::redis::Redis>(opts);
        
        std::cout << "Pinging Redis..." << std::endl;
        redis->ping();
        
        std::cout << "Redis connection successful!" << std::endl;
        
        // 测试创建多个连接
        std::cout << "Creating multiple connections..." << std::endl;
        for (int i = 0; i < 5; ++i) {
            std::cout << "Creating connection " << (i+1) << "..." << std::endl;
            auto conn = std::make_shared<sw::redis::Redis>(opts);
            conn->ping();
            std::cout << "Connection " << (i+1) << " successful!" << std::endl;
        }
        
        std::cout << "All connections successful!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
}