/**
 * @file example_usage.cpp
 * @brief 通用工具类使用示例
 * @details 展示 SignalHandler, CLIParser 和扩展的 ConfigManager 的使用方法
 */

#include "signal_handler.hpp"
#include "cli_parser.hpp"
#include "config_mgr.hpp"
#include <iostream>
#include <memory>

using namespace im::utils;

// 全局应用状态
struct AppConfig {
    std::string host = "localhost";
    int port = 8080;
    std::string log_level = "info";
    bool debug = false;
    std::string config_file = "app.json";
    std::string env_file = ".env";
};

AppConfig g_config;
std::unique_ptr<ConfigManager> g_config_mgr;

// 优雅关闭回调
void gracefulShutdown(int signal, const std::string& signal_name) {
    std::cout << "Received " << signal_name << ", shutting down gracefully..." << std::endl;
    
    // 执行清理操作
    if (g_config_mgr) {
        // 可以在这里保存配置或执行其他清理
        std::cout << "Saving configuration..." << std::endl;
    }
    
    std::cout << "Cleanup completed." << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // ==================== 1. 命令行参数解析 ====================
        CLIParser parser("example_app", "Example application using common utilities");
        
        // 添加版本信息
        parser.addVersionArgument("1.0.0");
        
        // 添加应用参数
        parser.addArgument("host", 'H', ArgumentType::STRING, false, 
                          "Server host address", "localhost", "Network",
                          [](const std::string& value) -> bool {
                              g_config.host = value;
                              std::cout << "Host set to: " << value << std::endl;
                              return true;
                          });
        
        parser.addArgument("port", 'p', ArgumentType::INTEGER, false,
                          "Server port number", "8080", "Network",
                          [](const std::string& value) -> bool {
                              try {
                                  g_config.port = std::stoi(value);
                                  std::cout << "Port set to: " << g_config.port << std::endl;
                                  return true;
                              } catch (...) {
                                  std::cerr << "Invalid port number: " << value << std::endl;
                                  return false;
                              }
                          });
        
        parser.addArgument("log-level", 'l', ArgumentType::STRING, false,
                          "Logging level", "info", "Logging",
                          [](const std::string& value) -> bool {
                              if (value == "debug" || value == "info" || value == "warn" || value == "error") {
                                  g_config.log_level = value;
                                  std::cout << "Log level set to: " << value << std::endl;
                                  return true;
                              } else {
                                  std::cerr << "Invalid log level: " << value << std::endl;
                                  return false;
                              }
                          });
        
        parser.addArgument("debug", 'd', ArgumentType::FLAG, false,
                          "Enable debug mode", "", "Logging",
                          [](const std::string&) -> bool {
                              g_config.debug = true;
                              std::cout << "Debug mode enabled" << std::endl;
                              return true;
                          });
        
        parser.addArgument("config", 'c', ArgumentType::STRING, false,
                          "Configuration file path", "app.json", "Configuration",
                          [](const std::string& value) -> bool {
                              g_config.config_file = value;
                              std::cout << "Config file set to: " << value << std::endl;
                              return true;
                          });
        
        parser.addArgument("env-file", 'e', ArgumentType::STRING, false,
                          "Environment file path", ".env", "Configuration",
                          [](const std::string& value) -> bool {
                              g_config.env_file = value;
                              std::cout << "Environment file set to: " << value << std::endl;
                              return true;
                          });
        
        // 解析命令行参数
        auto parse_result = parser.parse(argc, argv);
        if (!parse_result.success) {
            std::cerr << "Error: " << parse_result.error_message << std::endl;
            return 1;
        }
        
        // ==================== 2. 配置管理 ====================
        std::cout << "\n=== Loading Configuration ===" << std::endl;
        
        // 创建配置管理器，同时加载配置文件和环境变量
        g_config_mgr = std::make_unique<ConfigManager>(g_config.config_file, true, "APP");
        
        // 加载 .env 文件
        if (!g_config.env_file.empty()) {
            bool env_loaded = g_config_mgr->loadEnvFile(g_config.env_file, true);
            std::cout << "Environment file (" << g_config.env_file << ") loaded: " 
                      << (env_loaded ? "SUCCESS" : "FAILED") << std::endl;
        }
        
        // 从预定义列表加载环境变量
        std::vector<std::string> app_env_vars = {
            "HOST", "PORT", "LOG_LEVEL", "DEBUG", "DATABASE_URL"
        };
        g_config_mgr->loadEnvironmentVariables(app_env_vars, "MYAPP");
        
        // 展示配置优先级：环境变量 > 配置文件 > 默认值
        std::cout << "\n=== Final Configuration ===" << std::endl;
        
        // 使用环境变量优先的方式获取配置
        std::string final_host = g_config_mgr->getWithEnv<std::string>("host", "APP_HOST", g_config.host);
        int final_port = g_config_mgr->getWithEnv<int>("port", "APP_PORT", g_config.port);
        std::string final_log_level = g_config_mgr->getWithEnv<std::string>("log_level", "APP_LOG_LEVEL", g_config.log_level);
        bool final_debug = g_config_mgr->getWithEnv<bool>("debug", "APP_DEBUG", g_config.debug);
        
        std::cout << "Host: " << final_host << std::endl;
        std::cout << "Port: " << final_port << std::endl;
        std::cout << "Log Level: " << final_log_level << std::endl;
        std::cout << "Debug: " << (final_debug ? "enabled" : "disabled") << std::endl;
        
        // 直接从环境变量获取值
        std::string database_url = g_config_mgr->getEnv<std::string>("DATABASE_URL", "sqlite://app.db");
        std::cout << "Database URL: " << database_url << std::endl;
        
        // ==================== 3. 信号处理 ====================
        std::cout << "\n=== Setting up Signal Handlers ===" << std::endl;
        
        // 方式1：使用单例模式
        auto& signal_handler = SignalHandler::getInstance();
        signal_handler.registerGracefulShutdown(gracefulShutdown);
        
        // 方式2：使用RAII包装器（注释掉避免冲突）
        // ScopedSignalHandler scoped_handler(gracefulShutdown);
        
        // 注册自定义信号处理器
        signal_handler.registerSignalHandler(SIGUSR1, 
            [](int signal, const std::string& signal_name) {
                std::cout << "Received " << signal_name << " - Reloading configuration..." << std::endl;
                // 这里可以重新加载配置
            }, "SIGUSR1");
        
        std::cout << "Signal handlers registered for: ";
        auto registered_signals = signal_handler.getRegisteredSignals();
        for (size_t i = 0; i < registered_signals.size(); ++i) {
            std::cout << registered_signals[i];
            if (i < registered_signals.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        
        // ==================== 4. 应用主循环 ====================
        std::cout << "\n=== Application Started ===" << std::endl;
        std::cout << "Server running on " << final_host << ":" << final_port << std::endl;
        std::cout << "Send SIGUSR1 (kill -USR1 " << getpid() << ") to reload config" << std::endl;
        std::cout << "Press Ctrl+C to shutdown gracefully" << std::endl;
        
        // 模拟应用主循环
        int counter = 0;
        while (!signal_handler.isShutdownRequested()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << "App running... (counter: " << ++counter << ")" << std::endl;
            
            // 模拟一些工作
            if (counter >= 20) { // 100秒后自动退出（用于演示）
                std::cout << "Auto-shutdown after demo period" << std::endl;
                break;
            }
        }
        
        std::cout << "\n=== Application Shutdown ===" << std::endl;
        
        // 清理信号处理器
        signal_handler.cleanup();
        
        std::cout << "Application terminated successfully." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown application error occurred" << std::endl;
        return 1;
    }
    
    return 0;
}

/*
使用示例：

1. 基本运行：
   ./example_app

2. 指定参数：
   ./example_app --host 0.0.0.0 --port 9090 --debug --log-level debug

3. 使用环境变量：
   export APP_HOST=127.0.0.1
   export APP_PORT=8888
   ./example_app

4. 使用 .env 文件：
   echo "APP_HOST=192.168.1.100" > .env
   echo "APP_PORT=7777" >> .env
   ./example_app --env-file .env

5. 查看帮助：
   ./example_app --help

6. 查看版本：
   ./example_app --version

7. 测试信号处理：
   ./example_app &
   kill -USR1 $!  # 发送重载配置信号
   kill -INT $!   # 发送优雅关闭信号
*/