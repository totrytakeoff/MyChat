/**
 * @file main_example.cpp
 * @brief Gateway服务器主函数示例 - 使用通用工具类
 * @details 展示如何使用 SignalHandler, CLIParser 和 ConfigManager 重构 gateway/main.cpp
 */

#include "../common/utils/cli_parser.hpp"
#include "../common/utils/config_mgr.hpp"
#include "../common/utils/signal_handler.hpp"

#include "../common/database/redis_mgr.hpp"
#include "../common/utils/log_manager.hpp"
#include "./gateway_server/gateway_server.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <unistd.h>    // for getpid(), fork(), setsid(), chdir()
#include <sys/stat.h>  // for file permissions
#include <cstdlib>     // for exit()

using namespace im::utils;
using namespace im::gateway;

// 全局配置结构
struct GatewayConfig {
    std::string platform_config = "auth_config.json";
    std::string router_config = "router_config.json";
    std::string env_file = ".env";
    uint16_t ws_port = 8101;
    uint16_t http_port = 8102;
    std::string log_level = "info";
    bool daemon_mode = false;
    std::string pid_file;
    bool show_help = false;
    bool show_version = false;
};

// 全局变量
GatewayConfig g_config;
std::unique_ptr<ConfigManager> g_config_mgr;
std::shared_ptr<GatewayServer> g_server;

/**
 * @brief 优雅关闭回调函数（仅置位标志，实际清理在主线程完成）
 */
static std::atomic<bool> g_shutdown_requested_flag{false};
void gracefulShutdown(int signal, const std::string& signal_name) {
    (void)signal;
    std::cout << "\n=== Received " << signal_name << " signal ===" << std::endl;
    g_shutdown_requested_flag.store(true);
}

/**
 * @brief 设置命令行参数解析器
 */
void setupCLIParser(CLIParser& parser) {
    // 版本信息
    parser.addArgument("version", 'v', ArgumentType::FLAG, false, "Show version information", "", 
                       "General", [](const std::string&) -> bool {
                           std::cout << "Gateway Server v1.0.0" << std::endl;
                           std::cout << "Distributed IM Gateway Service" << std::endl;
                           exit(0);
                           return true;
                       });

    // 网络配置
    parser.addArgument("ws-port", 'w', ArgumentType::INTEGER, false, "WebSocket server port",
                       "8101", "Network", [](const std::string& value) -> bool {
                           try {
                               g_config.ws_port = static_cast<uint16_t>(std::stoi(value));
                               return true;
                           } catch (...) {
                               std::cerr << "Invalid WebSocket port: " << value << std::endl;
                               return false;
                           }
                       });

    parser.addArgument("http-port", 'H', ArgumentType::INTEGER, false, "HTTP server port", "8102",
                       "Network", [](const std::string& value) -> bool {
                           try {
                               g_config.http_port = static_cast<uint16_t>(std::stoi(value));
                               return true;
                           } catch (...) {
                               std::cerr << "Invalid HTTP port: " << value << std::endl;
                               return false;
                           }
                       });

    // 配置文件
    parser.addArgument("platform-config", 'p', ArgumentType::STRING, false,
                       "Platform authentication config file", "auth_config.json", "Configuration",
                       [](const std::string& value) -> bool {
                           g_config.platform_config = value;
                           return true;
                       });

    parser.addArgument("router-config", 'r', ArgumentType::STRING, false,
                       "Router configuration file", "router_config.json", "Configuration",
                       [](const std::string& value) -> bool {
                           g_config.router_config = value;
                           return true;
                       });

    parser.addArgument("env-file", 'e', ArgumentType::STRING, false, "Environment variables file",
                       ".env", "Configuration", [](const std::string& value) -> bool {
                           g_config.env_file = value;
                           return true;
                       });

    // 日志配置
    parser.addArgument(
            "log-level", 'l', ArgumentType::STRING, false, "Log level (debug|info|warn|error)",
            "info", "Logging", [](const std::string& value) -> bool {
                if (value == "debug" || value == "info" || value == "warn" || value == "error") {
                    g_config.log_level = value;
                    return true;
                } else {
                    std::cerr << "Invalid log level: " << value << std::endl;
                    return false;
                }
            });

    // 进程管理
    parser.addArgument("daemon", 'd', ArgumentType::FLAG, false, "Run as daemon process", "",
                       "Process", [](const std::string&) -> bool {
                           g_config.daemon_mode = true;
                           return true;
                       });

    parser.addArgument("pid-file", 0, ArgumentType::STRING, false, "PID file path for daemon mode",
                       "", "Process", [](const std::string& value) -> bool {
                           g_config.pid_file = value;
                           return true;
                       });
}

/**
 * @brief 初始化配置管理器
 */
bool initializeConfiguration() {
    try {
        // 创建配置管理器，加载环境变量
        // 注意：这里应该使用实际存在的配置文件，或者处理文件不存在的情况
        std::string config_file = "gateway.json";
        if (!std::filesystem::exists(config_file)) {
            std::cout << "Warning: Config file " << config_file << " not found, using defaults" << std::endl;
        }
        g_config_mgr = std::make_unique<ConfigManager>(config_file, true, "GATEWAY");

        // 加载 .env 文件
        if (!g_config.env_file.empty() && std::filesystem::exists(g_config.env_file)) {
            bool env_loaded = g_config_mgr->loadEnvFile(g_config.env_file, true);
            std::cout << "Environment file loaded: " << (env_loaded ? "SUCCESS" : "FAILED")
                      << std::endl;
        }

        // 从环境变量更新配置（环境变量优先）
        g_config.ws_port =
                g_config_mgr->getWithEnv<int>("ws_port", "WS_PORT", g_config.ws_port);
        g_config.http_port =
                g_config_mgr->getWithEnv<int>("http_port", "HTTP_PORT", g_config.http_port);
        g_config.platform_config = g_config_mgr->getWithEnv<std::string>(
                "platform_config", "PLATFORM_CONFIG_PATH", g_config.platform_config);
        g_config.router_config = g_config_mgr->getWithEnv<std::string>(
                "router_config", "ROUTER_CONFIG_PATH", g_config.router_config);
        g_config.log_level = g_config_mgr->getWithEnv<std::string>("log_level", "LOG_LEVEL",
                                                                   g_config.log_level);

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Configuration initialization failed: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief 初始化Redis连接
 */
bool initializeRedis() {
    try {
        // 从配置获取Redis参数
        std::string redis_host =
                g_config_mgr->getWithEnv<std::string>("redis.host", "REDIS_HOST", "127.0.0.1");
        int redis_port = g_config_mgr->getWithEnv<int>("redis.port", "REDIS_PORT", 6379);
        std::string redis_password =
                g_config_mgr->getWithEnv<std::string>("redis.password", "REDIS_PASSWORD", "");
        int redis_db = g_config_mgr->getWithEnv<int>("redis.db", "REDIS_DB", 1);

        std::cout << "Initializing Redis connection..." << std::endl;
        std::cout << "Redis Server: " << redis_host << ":" << redis_port << " (DB:" << redis_db
                  << ")" << std::endl;

        std::cout << "Redis Password: " << (redis_password.empty() ? "NONE" : redis_password) << std::endl;
        // 初始化Redis管理器
        im::db::RedisConfig redis_config;
        redis_config.host = redis_host;
        redis_config.port = redis_port;
        redis_config.password = redis_password;
        redis_config.db = redis_db;
        redis_config.pool_size = 10;
        redis_config.connect_timeout = 1000;
        redis_config.socket_timeout = 1000;

        if (!im::db::RedisManager::GetInstance().initialize(redis_config)) {
            std::cerr << "Failed to initialize Redis connection" << std::endl;
            return false;
        }

        std::cout << "✓ Redis connection established successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Redis initialization failed: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief 打印启动横幅
 */
void printStartupBanner() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    Gateway Server v1.0.0                     ║\n";
    std::cout << "║                Distributed IM Gateway Service                ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    std::cout << "=== Gateway Server Configuration ===" << std::endl;
    std::cout << "Service Name: gateway_server v1.0.0" << std::endl;
    std::cout << "Environment: production" << std::endl;
    std::cout << "WebSocket Port: " << g_config.ws_port << std::endl;
    std::cout << "HTTP Port: " << g_config.http_port << std::endl;
    std::cout << "Platform Config: " << g_config.platform_config << std::endl;
    std::cout << "Router Config: " << g_config.router_config << std::endl;
    std::cout << "Log Level: " << g_config.log_level << std::endl;
    std::cout << "Daemon Mode: " << (g_config.daemon_mode ? "ON" : "OFF") << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;
}

/**
 * @brief 创建PID文件
 */
bool createPidFile() {
    if (g_config.pid_file.empty()) {
        return true;
    }

    try {
        std::ofstream pid_file(g_config.pid_file);
        if (pid_file.is_open()) {
            pid_file << getpid() << std::endl;
            std::cout << "PID file created: " << g_config.pid_file << std::endl;
            return true;
        } else {
            std::cerr << "Failed to create PID file: " << g_config.pid_file << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating PID file: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief 后台化进程
 */
bool daemonize() {
    if (!g_config.daemon_mode) {
        return true;
    }

    std::cout << "Starting daemon mode..." << std::endl;

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        return false;
    }

    if (pid > 0) {
        // 父进程退出
        exit(0);
    }

    // 子进程继续
    if (setsid() < 0) {
        std::cerr << "setsid failed" << std::endl;
        return false;
    }

    // 改变工作目录到根目录（安全考虑）
    if (chdir("/") < 0) {
        std::cerr << "chdir failed" << std::endl;
        return false;
    }

    // 设置文件创建掩码
    umask(0);

    // 关闭标准输入输出（生产环境建议启用）
    // 注意：关闭后将无法看到控制台输出，需要确保日志系统正常工作
    // close(STDIN_FILENO);
    // close(STDOUT_FILENO);
    // close(STDERR_FILENO);

    std::cout << "Daemon started successfully" << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    try {
        // ==================== 1. 命令行参数解析 ====================
        CLIParser parser("gateway_server", "Distributed IM Gateway Service");
        setupCLIParser(parser);

        auto parse_result = parser.parse(argc, argv);
        if (!parse_result.success) {
            std::cerr << "Error: " << parse_result.error_message << std::endl;
            return 1;
        }

        // ==================== 2. 配置初始化 ====================
        if (!initializeConfiguration()) {
            return 1;
        }

        // 验证必要的配置文件是否存在
        if (!std::filesystem::exists(g_config.platform_config)) {
            std::cerr << "Error: Platform config file not found: " << g_config.platform_config << std::endl;
            return 1;
        }
        if (!std::filesystem::exists(g_config.router_config)) {
            std::cerr << "Error: Router config file not found: " << g_config.router_config << std::endl;
            return 1;
        }

        // ==================== 3. 打印启动信息 ====================
        printStartupBanner();

        // ==================== 4. 后台化 ====================
        if (!daemonize()) {
            return 1;
        }

        // ==================== 5. 创建PID文件 ====================
        if (!createPidFile()) {
            return 1;
        }

        // ==================== 6. 初始化Redis ====================
        if (!initializeRedis()) {
            return 1;
        }

        // ==================== 7. 设置日志级别 ====================
        std::cout << "Setting log level to: " << g_config.log_level << std::endl;
        LogManager::SetLogLevel(g_config.log_level);
        

        // ==================== 8. 创建和启动服务器 ====================
        std::cout << "Creating gateway server..." << std::endl;

        g_server = std::make_shared<GatewayServer>(g_config.platform_config,
                                                   g_config.router_config,
                                                   g_config.ws_port,
                                                   g_config.http_port);

        // 注册示例消息处理器（如果需要）
        // registerSampleHandlers(g_server);

        // ==================== 9. 设置信号处理 ====================
        auto& signal_handler = SignalHandler::getInstance();
        bool signal_registered = signal_handler.registerGracefulShutdown(gracefulShutdown);
        if (!signal_registered) {
            std::cerr << "Warning: Failed to register some signal handlers" << std::endl;
        }
        std::cout << "Signal handlers registered (SIGINT, SIGTERM, SIGQUIT)" << std::endl;

        // ==================== 10. 启动服务器 ====================
        std::cout << "Starting gateway server..." << std::endl;
        g_server->start();
        std::cout << "✓ Gateway server started successfully!" << std::endl;

        // 显示服务器信息
        std::cout << "WebSocket Server: ws://0.0.0.0:" << g_config.ws_port << std::endl;
        std::cout << "HTTP Server: http://0.0.0.0:" << g_config.http_port << std::endl;
        std::cout << "Online Users: " << g_server->get_online_count() << std::endl;

        // ==================== 11. 等待关闭信号 ====================
        if (!g_config.daemon_mode) {
            signal_handler.waitForShutdown("Server is running. Press Ctrl+C to shutdown...");
        } else {
            // 守护进程模式：循环等待
            while (!signal_handler.isShutdownRequested()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        // ==================== 12. 清理和退出 ====================
        std::cout << "\nShutting down gracefully..." << std::endl;

        if (g_server) {
            g_server->stop();
        }

        // 清理PID文件
        if (!g_config.pid_file.empty() && std::filesystem::exists(g_config.pid_file)) {
            std::filesystem::remove(g_config.pid_file);
            std::cout << "PID file removed: " << g_config.pid_file << std::endl;
        }

        std::cout << "Gateway server shutdown complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;

        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;

        return 1;
    }

    return 0;
}

/*
使用示例：

1. 基本运行：
   ./gateway_server

2. 指定端口和配置：
   ./gateway_server --ws-port 9001 --http-port 9002 --platform-config auth.json

3. 后台运行：
   ./gateway_server --daemon --pid-file /var/run/gateway.pid

4. 使用环境变量：
   export GATEWAY_WS_PORT=8201
   export GATEWAY_HTTP_PORT=8202
   ./gateway_server

5. 使用.env文件：
   echo "GATEWAY_WS_PORT=8301" > .env
   echo "GATEWAY_HTTP_PORT=8302" >> .env
   ./gateway_server --env-file .env

6. 查看帮助：
   ./gateway_server --help

7. 查看版本：
   ./gateway_server --version

8. 调试模式：
   ./gateway_server --log-level debug
*/