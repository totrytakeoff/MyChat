#include <chrono>
#include <iostream>
#include <thread>

#include "../../common/database/redis_mgr.hpp"
#include "../../gateway/gateway_server/gateway_server.hpp"

using namespace std;
using nlohmann::json;
int main() {
    try {
        // Ensure Redis is healthy with correct credentials for this environment
        im::db::RedisConfig redis_cfg;
        redis_cfg.host = "127.0.0.1";
        redis_cfg.port = 6379;
        redis_cfg.password = "myself";  // adjust to your local redis password
        redis_cfg.db = 1;
        im::db::RedisManager::GetInstance().initialize(redis_cfg);

        uint16_t ws_port = 8101;
        uint16_t http_port = 8102;
        const std::string platform_cfg = "../test_auth_config.json";
        const std::string router_cfg = "../test_router_config.json";

        auto auth_mgr = std::make_shared<im::gateway::MultiPlatformAuthManager>(platform_cfg);


        auto server = std::make_shared<im::gateway::GatewayServer>(platform_cfg, router_cfg,
                                                                   ws_port, http_port);


        server->register_message_handlers(
                1004, [](const im::gateway::UnifiedMessage& msg) -> im::gateway::ProcessorResult {
                    json j;
                    j["code"] = 200;
                    j["body"] = "admin user info";
                    j["err_msg"] = "ok";
                    return im::gateway::ProcessorResult(0, "OK", "", j.dump());
                });
        server->start();

        auto token = auth_mgr->generate_access_token("1", "admin", "test-dev", "Windows", 600);
        httplib::Client cli("127.0.0.1", http_port);
        cli.set_keep_alive(false);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(30, 0);  // 增加超时时间至30秒，确保服务器响应
        cli.set_write_timeout(5, 0);

        // 注意这里的headers，需要设置Authorization，X-Device-ID，X-Platform三个字段进行验证
        httplib::Headers headers = {{"Authorization", std::string("Bearer ") + token},
                                    {"X-Device-ID", "test-dev"},
                                    {"X-Platform", "Windows"}};

        std::cout << "Gateway server running on http://127.0.0.1:" << http_port << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl;
        std::cout << "temp token: " << token << std::endl;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            auto res = cli.Post("/api/v1/auth/info", headers, "{}", "application/json");
            cout << "Response status: " << (res ? res->status : -1) << endl;
            if (res && res->body.size() > 0) cout << "Response body: " << res->body << endl;

            if (!res) {
                std::cout << "http req no response!" << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
