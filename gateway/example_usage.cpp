/**
 * @file example_usage.cpp
 * @brief GatewayServer使用示例
 * 
 * 展示如何正确使用修复后的GatewayServer
 */

#include "gateway_server/gateway_server.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace im::gateway;

int main() {
    try {
        std::cout << "=== MyChat Gateway Server Example ===" << std::endl;
        
        // 创建GatewayServer实例
        // 参数：平台配置路径, 路由配置路径, 认证配置路径, WebSocket端口, HTTP端口
        auto gateway = std::make_shared<GatewayServer>(
            "config/platform.json",  // 平台配置
            "config/router.json",    // 路由配置  
            "config/auth.json",      // 认证配置
            8080,                    // WebSocket端口
            8081                     // HTTP端口
        );
        
        std::cout << "Gateway server created successfully" << std::endl;
        
        // 启动服务器
        gateway->start();
        std::cout << "Gateway server started" << std::endl;
        
        // 打印服务器状态
        std::cout << "\n" << gateway->get_server_stats() << std::endl;
        
        // 模拟运行一段时间
        std::cout << "Server running... Press Ctrl+C to stop" << std::endl;
        
        // 定期打印统计信息
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << "\n=== Server Status ===" << std::endl;
            std::cout << "Running: " << (gateway->is_running() ? "Yes" : "No") << std::endl;
            std::cout << "Online users: " << gateway->get_online_count() << std::endl;
        }
        
        // 停止服务器
        std::cout << "\nStopping server..." << std::endl;
        gateway->stop();
        
        std::cout << "Server stopped successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}