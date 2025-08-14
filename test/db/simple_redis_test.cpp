#include <iostream>
#include <sw/redis++/redis++.h>

int main() {
    try {
        std::cout << "Creating Redis connection options..." << std::endl;
        
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.password = "myself";
        opts.db = 1;
        opts.connect_timeout = std::chrono::milliseconds(2000);
        opts.socket_timeout = std::chrono::milliseconds(2000);
        
        std::cout << "Creating Redis connection..." << std::endl;
        auto redis = std::make_shared<sw::redis::Redis>(opts);
        std::cout << "Redis connection created successfully" << std::endl;
        
        std::cout << "Testing ping..." << std::endl;
        redis->ping();
        std::cout << "Ping successful" << std::endl;
        
        std::cout << "Setting test key..." << std::endl;
        redis->set("test:simple", "hello");
        std::cout << "Set operation successful" << std::endl;
        
        std::cout << "Getting test key..." << std::endl;
        auto result = redis->get("test:simple");
        if (result) {
            std::cout << "Got value: " << *result << std::endl;
        } else {
            std::cout << "No value found" << std::endl;
        }
        
        std::cout << "All tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}