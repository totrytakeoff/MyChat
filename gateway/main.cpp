#include "../common/database/redis/redis_mgr.hpp"
#include "../common/utils/cli_parser.hpp"
#include "../common/utils/config_mgr.hpp"
#include "../common/utils/log_manager.hpp"
#include "../common/utils/signal_handler.hpp"
#include "gateway_server/gateway_server.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

struct GatewayRuntimeConfig {
    std::string config_file = "config/dev.json";
    uint16_t websocket_port = 8080;
    uint16_t http_port = 8081;
    std::string log_level = "info";
};

GatewayRuntimeConfig g_config;
im::gateway::GatewayServer* g_server = nullptr;

uint16_t read_port(const im::utils::ConfigManager& config,
                   const std::string& config_key,
                   const std::string& env_key,
                   uint16_t default_value) {
    int port = config.getWithEnv<int>(config_key, env_key, default_value);
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("Invalid port for " + config_key + ": " + std::to_string(port));
    }
    return static_cast<uint16_t>(port);
}

void configure_cli(im::utils::CLIParser& parser) {
    parser.addArgument("config", 'c', im::utils::ArgumentType::STRING, false,
                       "Gateway config file", g_config.config_file, "Configuration",
                       [](const std::string& value) {
                           g_config.config_file = value;
                           return true;
                       });
    parser.addArgument("ws-port", 'w', im::utils::ArgumentType::INTEGER, false,
                       "WebSocket server port", "", "Network", [](const std::string& value) {
                           g_config.websocket_port = static_cast<uint16_t>(std::stoi(value));
                           return true;
                       });
    parser.addArgument("http-port", 'H', im::utils::ArgumentType::INTEGER, false,
                       "HTTP server port", "", "Network", [](const std::string& value) {
                           g_config.http_port = static_cast<uint16_t>(std::stoi(value));
                           return true;
                       });
    parser.addArgument("log-level", 'l', im::utils::ArgumentType::STRING, false,
                       "Log level: debug, info, warn, error", g_config.log_level, "Logging",
                       [](const std::string& value) {
                           g_config.log_level = value;
                           return true;
                       });
}

im::db::RedisConfig load_redis_config(const im::utils::ConfigManager& config) {
    im::db::RedisConfig redis_config;
    redis_config.host =
            config.getWithEnv<std::string>("redis.host", "MYCHAT_REDIS_HOST", redis_config.host);
    redis_config.port = config.getWithEnv<int>("redis.port", "MYCHAT_REDIS_PORT",
                                               redis_config.port);
    redis_config.password = config.getWithEnv<std::string>(
            "redis.password", "MYCHAT_REDIS_PASSWORD", redis_config.password);
    redis_config.db = config.getWithEnv<int>("redis.db", "MYCHAT_REDIS_DB", redis_config.db);
    redis_config.pool_size = config.getWithEnv<int>("redis.pool_size", "MYCHAT_REDIS_POOL_SIZE",
                                                    redis_config.pool_size);
    redis_config.connect_timeout = config.getWithEnv<int>(
            "redis.connect_timeout", "MYCHAT_REDIS_CONNECT_TIMEOUT", redis_config.connect_timeout);
    redis_config.socket_timeout = config.getWithEnv<int>(
            "redis.socket_timeout", "MYCHAT_REDIS_SOCKET_TIMEOUT", redis_config.socket_timeout);
    return redis_config;
}

bool initialize_redis(const im::utils::ConfigManager& config) {
    auto redis_config = load_redis_config(config);
    if (!im::db::RedisManager::GetInstance().initialize(redis_config)) {
        std::cerr << "Failed to initialize Redis at " << redis_config.host << ":"
                  << redis_config.port << std::endl;
        return false;
    }
    return true;
}

void shutdown_gateway(int, const std::string&) {
    if (g_server) {
        g_server->stop();
    }
    im::db::RedisManager::GetInstance().shutdown();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        im::utils::CLIParser parser("gateway_server", "MyChat gateway server");
        configure_cli(parser);

        auto parse_result = parser.parse(argc, argv);
        if (!parse_result.success) {
            std::cerr << parse_result.error_message << std::endl;
            return 1;
        }

        if (!std::filesystem::exists(g_config.config_file)) {
            std::cerr << "Config file does not exist: " << g_config.config_file << std::endl;
            return 1;
        }

        im::utils::ConfigManager config(g_config.config_file);
        g_config.websocket_port = read_port(config, "gateway.websocket_port",
                                            "MYCHAT_GATEWAY_WS_PORT", g_config.websocket_port);
        g_config.http_port = read_port(config, "gateway.http_port", "MYCHAT_GATEWAY_HTTP_PORT",
                                       g_config.http_port);
        g_config.log_level =
                config.getWithEnv<std::string>("gateway.log_level", "MYCHAT_LOG_LEVEL",
                                               g_config.log_level);

        im::utils::LogManager::SetLogLevel(g_config.log_level);
        if (!initialize_redis(config)) {
            return 1;
        }

        auto server = std::make_shared<im::gateway::GatewayServer>(
                g_config.config_file, g_config.config_file, g_config.websocket_port,
                g_config.http_port);
        g_server = server.get();

        auto& signal_handler = im::utils::SignalHandler::getInstance();
        signal_handler.registerGracefulShutdown(shutdown_gateway);

        server->start();
        std::cout << "Gateway server started. WebSocket port: " << g_config.websocket_port
                  << ", HTTP port: " << g_config.http_port << std::endl;
        signal_handler.waitForShutdown("Press Ctrl+C to shutdown gateway_server...");
        signal_handler.cleanup();
        if (server->is_running()) {
            server->stop();
        }
        g_server = nullptr;
        server.reset();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "gateway_server failed: " << e.what() << std::endl;
        im::utils::SignalHandler::getInstance().cleanup();
        return 1;
    }
}
