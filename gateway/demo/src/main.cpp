#include "gateway_server.hpp"
#include <iostream>
#include <signal.h>
#include <memory>

// 全局的网关服务器实例
std::unique_ptr<GatewayServer> g_gateway_server;

// 信号处理函数
void SignalHandler(int signal) {
    std::cout << "\nReceived signal: " << signal << std::endl;
    
    if (g_gateway_server) {
        std::cout << "Shutting down gateway server..." << std::endl;
        g_gateway_server->Stop();
        g_gateway_server.reset();
    }
    
    std::cout << "Gateway server stopped. Goodbye!" << std::endl;
    exit(0);
}

// 打印使用说明
void PrintUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [config_file]" << std::endl;
    std::cout << "  config_file: Path to gateway configuration file" << std::endl;
    std::cout << "               Default: config/gateway_config.json" << std::endl;
    std::cout << std::endl;
    std::cout << "MyChat Gateway Server - A demo implementation" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  - WebSocket and HTTP dual protocol support" << std::endl;
    std::cout << "  - Token-based authentication" << std::endl;
    std::cout << "  - Request routing to microservices" << std::endl;
    std::cout << "  - Real-time message pushing" << std::endl;
    std::cout << "  - Connection and health monitoring" << std::endl;
    std::cout << std::endl;
    std::cout << "Demo API endpoints:" << std::endl;
    std::cout << "  POST /api/v1/login        - User login" << std::endl;
    std::cout << "  POST /api/v1/logout       - User logout" << std::endl;
    std::cout << "  GET  /api/v1/user/info    - Get user info" << std::endl;
    std::cout << "  POST /api/v1/message/send - Send message" << std::endl;
    std::cout << "  GET  /health              - Health check" << std::endl;
    std::cout << "  GET  /stats               - Server statistics" << std::endl;
}

// 打印启动信息
void PrintStartupInfo() {
    std::cout << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << "     MyChat Gateway Server Demo" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build Date: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << std::endl;
    std::cout << "Demo Login Credentials:" << std::endl;
    std::cout << "  Username: test" << std::endl;
    std::cout << "  Password: test" << std::endl;
    std::cout << std::endl;
    std::cout << "Features demonstrated:" << std::endl;
    std::cout << "  ✓ HTTP + WebSocket dual protocol" << std::endl;
    std::cout << "  ✓ Token authentication" << std::endl;
    std::cout << "  ✓ Request routing" << std::endl;
    std::cout << "  ✓ Connection management" << std::endl;
    std::cout << "  ✓ Health monitoring" << std::endl;
    std::cout << "  ✓ Statistics collection" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << std::endl;
}

// 实时状态监控
void StartStatusMonitor() {
    static auto start_time = std::chrono::system_clock::now();
    
    std::thread([start_time]() {
        while (g_gateway_server && g_gateway_server->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
            if (!g_gateway_server || !g_gateway_server->IsRunning()) {
                break;
            }
            
            auto now = std::chrono::system_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
            
            std::cout << "[" << std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count() << "] ";
            std::cout << "Gateway running... (uptime: " << uptime.count() << "s)" << std::endl;
        }
    }).detach();
}

int main(int argc, char* argv[]) {
    try {
        // 解析命令行参数
        std::string config_path = "config/gateway_config.json";
        
        if (argc > 1) {
            if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
                PrintUsage(argv[0]);
                return 0;
            }
            config_path = argv[1];
        }
        
        // 打印启动信息
        PrintStartupInfo();
        
        // 注册信号处理
        signal(SIGINT, SignalHandler);   // Ctrl+C
        signal(SIGTERM, SignalHandler);  // 终止信号
        #ifdef SIGQUIT
        signal(SIGQUIT, SignalHandler);  // Quit信号
        #endif
        
        std::cout << "Starting MyChat Gateway Server..." << std::endl;
        std::cout << "Configuration file: " << config_path << std::endl;
        std::cout << std::endl;
        
        // 创建并启动网关服务器
        g_gateway_server = std::make_unique<GatewayServer>();
        
        if (!g_gateway_server->Start(config_path)) {
            std::cerr << "❌ Failed to start gateway server!" << std::endl;
            std::cerr << "Please check:" << std::endl;
            std::cerr << "  1. Configuration file exists and is valid" << std::endl;
            std::cerr << "  2. Required ports are not in use" << std::endl;
            std::cerr << "  3. Log files for more details" << std::endl;
            return -1;
        }
        
        std::cout << "✅ MyChat Gateway Server started successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "Service endpoints:" << std::endl;
        std::cout << "  🌐 HTTP Server:     http://localhost:8080" << std::endl;
        std::cout << "  🔌 WebSocket Server: ws://localhost:8081" << std::endl;
        std::cout << std::endl;
        std::cout << "Quick test commands:" << std::endl;
        std::cout << "  curl http://localhost:8080/health" << std::endl;
        std::cout << "  curl http://localhost:8080/stats" << std::endl;
        std::cout << "  curl -X POST http://localhost:8080/api/v1/login \\" << std::endl;
        std::cout << "       -H 'Content-Type: application/json' \\" << std::endl;
        std::cout << "       -d '{\"username\":\"test\",\"password\":\"test\"}'" << std::endl;
        std::cout << std::endl;
        std::cout << "📝 Logs are written to the 'logs/' directory" << std::endl;
        std::cout << "📊 Press Ctrl+C to stop the server gracefully" << std::endl;
        std::cout << std::endl;
        
        // 启动状态监控
        StartStatusMonitor();
        
        // 主线程等待
        while (g_gateway_server && g_gateway_server->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 可以在这里添加额外的主循环逻辑
            // 比如定期检查系统状态、处理管理员命令等
        }
        
        std::cout << "Gateway server main loop ended." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Gateway server fatal error: " << e.what() << std::endl;
        std::cerr << "The server will now exit." << std::endl;
        
        if (g_gateway_server) {
            g_gateway_server->Stop();
        }
        
        return -1;
        
    } catch (...) {
        std::cerr << "❌ Gateway server encountered an unknown error!" << std::endl;
        std::cerr << "The server will now exit." << std::endl;
        
        if (g_gateway_server) {
            g_gateway_server->Stop();
        }
        
        return -1;
    }
    
    return 0;
}