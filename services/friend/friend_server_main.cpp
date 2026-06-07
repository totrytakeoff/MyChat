#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "../../common/utils/cli_parser.hpp"
#include "../../common/utils/config_mgr.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../../common/utils/service_identity.hpp"
#include "../../common/utils/signal_handler.hpp"
#include "friend_server_app.hpp"

namespace {

struct FriendServerRuntimeConfig {
    std::string config_file = "config/dev.json";
    std::string listen_address = "0.0.0.0:9003";
    std::string pg_host = "127.0.0.1";
    int pg_port = 5432;
    std::string pg_database = "mychat";
    std::string pg_user = "mychat";
    std::string pg_password = "mychat-dev-pass";
    std::string log_level = "info";
};

FriendServerRuntimeConfig g_config;
im::service::friend_::FriendServerApp* g_server = nullptr;

std::string build_pg_connection_string(const FriendServerRuntimeConfig& config) {
    std::string conn = "host=" + config.pg_host +
        " port=" + std::to_string(config.pg_port) +
        " dbname=" + config.pg_database +
        " user=" + config.pg_user;
    if (!config.pg_password.empty()) {
        conn += " password=" + config.pg_password;
    }
    return conn;
}

void configure_cli(im::utils::CLIParser& parser) {
    parser.addArgument("config", 'c', im::utils::ArgumentType::STRING, false,
                       "Friend server config file", g_config.config_file, "Configuration",
                       [](const std::string& value) {
                           g_config.config_file = value;
                           return true;
                       });
    parser.addArgument("listen", 'L', im::utils::ArgumentType::STRING, false,
                       "gRPC listen address, e.g. 0.0.0.0:9003", "", "Network",
                       [](const std::string& value) {
                           g_config.listen_address = value;
                           return true;
                       });
    parser.addArgument("log-level", 'l', im::utils::ArgumentType::STRING, false,
                       "Log level: debug, info, warn, error", g_config.log_level, "Logging",
                       [](const std::string& value) {
                           g_config.log_level = value;
                           return true;
                       });
}

void shutdown_friend_server(int, const std::string&) {
    if (g_server) {
        g_server->shutdown();
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        im::utils::CLIParser parser("friend_server", "MyChat Friend gRPC server");
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
        g_config.listen_address = config.getWithEnv<std::string>(
            "friend.listen_address", "MYCHAT_FRIEND_LISTEN_ADDRESS", g_config.listen_address);
        g_config.pg_host = config.getWithEnv<std::string>(
            "postgres.host", "MYCHAT_DB_HOST", g_config.pg_host);
        g_config.pg_port = config.getWithEnv<int>(
            "postgres.port", "MYCHAT_DB_PORT", g_config.pg_port);
        g_config.pg_database = config.getWithEnv<std::string>(
            "postgres.database", "MYCHAT_DB_DATABASE", g_config.pg_database);
        g_config.pg_user = config.getWithEnv<std::string>(
            "postgres.username", "MYCHAT_DB_USER", g_config.pg_user);
        g_config.pg_password = config.getWithEnv<std::string>(
            "postgres.password", "MYCHAT_DB_PASSWORD", g_config.pg_password);
        g_config.log_level = config.getWithEnv<std::string>(
            "friend.log_level", "MYCHAT_LOG_LEVEL", g_config.log_level);

        im::utils::LogManager::SetLogLevel(g_config.log_level);
        if (!im::utils::ServiceIdentityManager::getInstance().initializeFromEnv("friend")) {
            std::cerr << "Failed to initialize friend service identity" << std::endl;
            return 1;
        }

        im::service::friend_::FriendServerConfig server_config;
        server_config.listen_address = g_config.listen_address;
        server_config.postgres_connection_string = build_pg_connection_string(g_config);

        im::service::friend_::FriendServerApp server(server_config);
        g_server = &server;

        auto& signal_handler = im::utils::SignalHandler::getInstance();
        signal_handler.registerGracefulShutdown(shutdown_friend_server);

        if (!server.start()) {
            return 1;
        }

        std::cout << "Friend server started. gRPC listen address: "
                  << server.listen_address() << std::endl;
        signal_handler.waitForShutdown("Press Ctrl+C to shutdown friend_server...");
        signal_handler.cleanup();
        server.shutdown();
        g_server = nullptr;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "friend_server failed: " << e.what() << std::endl;
        im::utils::SignalHandler::getInstance().cleanup();
        return 1;
    }
}
