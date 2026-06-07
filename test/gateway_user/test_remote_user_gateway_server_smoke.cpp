#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <database/redis/redis_mgr.hpp>
#include <gateway/gateway_server/gateway_server.hpp>
#include <services/user/user_server_app.hpp>

#include "../support/postgres_schema.hpp"

namespace {

namespace net = boost::asio;
using json = nlohmann::json;
using tcp = net::ip::tcp;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::GatewayServer;
using im::service::user::UserServerApp;
using im::service::user::UserServerConfig;

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";
constexpr const char* kAccountPrefix = "remote-user-gateway-test-";

int find_free_port() {
    net::io_context ioc;
    tcp::acceptor acceptor(ioc);
    acceptor.open(tcp::v4());
    acceptor.set_option(net::socket_base::reuse_address(true));
    acceptor.bind(tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
    const int port = acceptor.local_endpoint().port();
    acceptor.close();
    return port;
}

RedisConfig test_redis_config() {
    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "mychat-dev-pass";
    config.db = 15;
    config.pool_size = 4;
    config.connect_timeout = 1000;
    config.socket_timeout = 1000;
    config.pool_wait_timeout = 1000;
    return config;
}

std::filesystem::path source_path(const std::string& relative) {
    return std::filesystem::path(MYCHAT_SOURCE_DIR) / relative;
}

void ensure_schema() {
    odb::pgsql::database db(kConnStr);
    im::test::EnsureCoreSchema(db);
}

void cleanup_users() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(std::string(R"(DELETE FROM "im_users" WHERE "account" LIKE ')") +
               kAccountPrefix + "%'");
    t.commit();
}

std::filesystem::path write_temp_config(int ws_port, int http_port, int user_port) {
    std::ifstream input(source_path("config/dev.json"));
    if (!input) {
        throw std::runtime_error("Failed to open config/dev.json");
    }
    json config = json::parse(input);

    config["gateway"]["websocket_port"] = ws_port;
    config["gateway"]["http_port"] = http_port;
    config["gateway"]["cert_file"] = source_path("test/network/test_cert.pem").string();
    config["gateway"]["key_file"] = source_path("test/network/test_key.pem").string();
    config["redis"]["db"] = 15;
    config["redis"]["pool_size"] = 4;
    config["redis"]["pool_wait_timeout"] = 1000;
    config["user"]["mode"] = "remote";
    config["user"]["remote_endpoint"] = "127.0.0.1:" + std::to_string(user_port);
    config["user"]["timeout_ms"] = 1000;
    config["push"]["mode"] = "local";

    auto out_path = std::filesystem::temp_directory_path() /
        ("mychat-remote-user-gateway-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    std::ofstream output(out_path);
    if (!output) {
        throw std::runtime_error("Failed to write temp Gateway config");
    }
    output << config.dump(2);
    return out_path;
}

class RemoteUserGatewayServerSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_schema();
        cleanup_users();
        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";
    }

    void TearDown() override {
        if (gateway_) {
            gateway_->stop();
            gateway_.reset();
        }
        user_server_.reset();
        cleanup_users();
        redis_manager().shutdown();
        if (!temp_config_.empty()) {
            std::error_code ec;
            std::filesystem::remove(temp_config_, ec);
        }
    }

    void start_stack() {
        UserServerConfig user_config;
        user_config.listen_address = "127.0.0.1:0";
        user_config.postgres_connection_string = kConnStr;
        user_server_ = std::make_unique<UserServerApp>(user_config);
        ASSERT_TRUE(user_server_->start());
        ASSERT_GT(user_server_->selected_port(), 0);

        ws_port_ = find_free_port();
        http_port_ = find_free_port();
        temp_config_ = write_temp_config(
            ws_port_, http_port_, user_server_->selected_port());
        gateway_ = std::make_unique<GatewayServer>(
            temp_config_.string(), temp_config_.string(), ws_port_, http_port_);
        gateway_->start();
        ASSERT_TRUE(wait_for_http_health());
    }

    bool wait_for_http_health() const {
        for (int i = 0; i < 50; ++i) {
            httplib::Client client("127.0.0.1", http_port_);
            client.set_connection_timeout(0, 100000);
            client.set_read_timeout(0, 200000);
            auto res = client.Get("/api/v1/health");
            if (res && res->status == 200) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    int ws_port_ = 0;
    int http_port_ = 0;
    std::filesystem::path temp_config_;
    std::unique_ptr<UserServerApp> user_server_;
    std::unique_ptr<GatewayServer> gateway_;
};

}  // namespace

TEST_F(RemoteUserGatewayServerSmokeTest, AuthHttpRoutesUseRemoteUserServer) {
    start_stack();

    httplib::Client client("127.0.0.1", http_port_);
    client.set_connection_timeout(0, 200000);
    client.set_read_timeout(1, 0);

    json register_body;
    register_body["account"] = std::string(kAccountPrefix) + "account";
    register_body["password"] = "remotePass123";
    register_body["nickname"] = "Remote Gateway User";
    auto reg = client.Post(
        "/api/v1/auth/register", register_body.dump(), "application/json");
    ASSERT_TRUE(reg) << "HTTP register request failed";
    ASSERT_EQ(reg->status, 201) << reg->body;
    auto reg_json = json::parse(reg->body);
    ASSERT_TRUE(reg_json.contains("access_token"));
    ASSERT_TRUE(reg_json.contains("profile"));
    EXPECT_EQ(reg_json["profile"]["account"], register_body["account"]);
    EXPECT_EQ(reg_json["profile"]["nickname"], "Remote Gateway User");

    json login_body;
    login_body["account"] = register_body["account"];
    login_body["password"] = "remotePass123";
    auto login = client.Post(
        "/api/v1/auth/login", login_body.dump(), "application/json");
    ASSERT_TRUE(login) << "HTTP login request failed";
    ASSERT_EQ(login->status, 200) << login->body;
    auto login_json = json::parse(login->body);
    ASSERT_TRUE(login_json.contains("access_token"));

    httplib::Headers headers = {
        {"Authorization", "Bearer " + login_json["access_token"].get<std::string>()},
    };
    auto profile = client.Get("/api/v1/auth/info", headers);
    ASSERT_TRUE(profile) << "HTTP profile request failed";
    ASSERT_EQ(profile->status, 200) << profile->body;
    auto profile_json = json::parse(profile->body);
    EXPECT_EQ(profile_json["profile"]["account"], register_body["account"]);
}
