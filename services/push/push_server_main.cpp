#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "../../common/utils/cli_parser.hpp"
#include "../../common/utils/config_mgr.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../../common/utils/service_identity.hpp"
#include "../../common/utils/signal_handler.hpp"
#include "push_server_app.hpp"

namespace {

struct PushServerRuntimeConfig {
    std::string config_file = "config/dev.json";
    std::string listen_address = "0.0.0.0:9101";
    std::string log_level = "info";
};

PushServerRuntimeConfig g_config;
im::service::push::PushServerApp* g_server = nullptr;

void configure_cli(im::utils::CLIParser& parser) {
    parser.addArgument("config", 'c', im::utils::ArgumentType::STRING, false,
                       "Push server config file", g_config.config_file, "Configuration",
                       [](const std::string& value) {
                           g_config.config_file = value;
                           return true;
                       });
    parser.addArgument("listen", 'L', im::utils::ArgumentType::STRING, false,
                       "gRPC listen address, e.g. 0.0.0.0:9101", "", "Network",
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

void shutdown_push_server(int, const std::string&) {
    if (g_server) {
        g_server->shutdown();
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        im::utils::CLIParser parser("push_server", "MyChat Push gRPC server");
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
            "push.listen_address", "MYCHAT_PUSH_LISTEN_ADDRESS", g_config.listen_address);
        g_config.log_level = config.getWithEnv<std::string>(
            "push.log_level", "MYCHAT_LOG_LEVEL", g_config.log_level);

        im::utils::LogManager::SetLogLevel(g_config.log_level);
        if (!im::utils::ServiceIdentityManager::getInstance().initializeFromEnv("push")) {
            std::cerr << "Failed to initialize push service identity" << std::endl;
            return 1;
        }

        im::service::push::PushServerConfig server_config;
        server_config.listen_address = g_config.listen_address;

        im::service::push::PushServerApp server(server_config);
        g_server = &server;

        auto& signal_handler = im::utils::SignalHandler::getInstance();
        signal_handler.registerGracefulShutdown(shutdown_push_server);

        if (!server.start()) {
            return 1;
        }

        std::cout << "Push server started. gRPC listen address: "
                  << server.listen_address() << std::endl;
        signal_handler.waitForShutdown("Press Ctrl+C to shutdown push_server...");
        signal_handler.cleanup();
        server.shutdown();
        g_server = nullptr;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "push_server failed: " << e.what() << std::endl;
        im::utils::SignalHandler::getInstance().cleanup();
        return 1;
    }
}
