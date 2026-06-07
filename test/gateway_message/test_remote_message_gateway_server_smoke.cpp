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
#include <message.hpp>
#include <services/message/message_server_app.hpp>

#include "../support/postgres_schema.hpp"

namespace {

namespace net = boost::asio;
using json = nlohmann::json;
using tcp = net::ip::tcp;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::GatewayServer;
using im::service::message::MessageServerApp;
using im::service::message::MessageServerConfig;

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";
constexpr const char* kUidPrefix = "remote-message-gateway-test-";

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

void cleanup_messages() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_messages"
        WHERE "sender_uid" LIKE 'remote-message-gateway-test-%'
           OR "receiver_uid" LIKE 'remote-message-gateway-test-%')");
    t.commit();
}

std::filesystem::path write_temp_config(int ws_port, int http_port, int message_port) {
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
    config["message"]["mode"] = "remote";
    config["message"]["remote_endpoint"] = "127.0.0.1:" + std::to_string(message_port);
    config["message"]["timeout_ms"] = 1000;
    config["user"]["mode"] = "local";
    config["push"]["mode"] = "local";

    auto out_path = std::filesystem::temp_directory_path() /
        ("mychat-remote-message-gateway-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    std::ofstream output(out_path);
    if (!output) {
        throw std::runtime_error("Failed to write temp Gateway config");
    }
    output << config.dump(2);
    return out_path;
}

class RemoteMessageGatewayServerSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_schema();
        cleanup_messages();
        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";
    }

    void TearDown() override {
        if (gateway_) {
            gateway_->stop();
            gateway_.reset();
        }
        message_server_.reset();
        cleanup_messages();
        redis_manager().shutdown();
        if (!temp_config_.empty()) {
            std::error_code ec;
            std::filesystem::remove(temp_config_, ec);
        }
    }

    void start_stack() {
        MessageServerConfig message_config;
        message_config.listen_address = "127.0.0.1:0";
        message_config.postgres_connection_string = kConnStr;
        message_server_ = std::make_unique<MessageServerApp>(message_config);
        ASSERT_TRUE(message_server_->start());
        ASSERT_GT(message_server_->selected_port(), 0);

        ws_port_ = find_free_port();
        http_port_ = find_free_port();
        temp_config_ = write_temp_config(
            ws_port_, http_port_, message_server_->selected_port());
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

    std::string make_access_token(const std::string& uid) {
        im::gateway::MultiPlatformAuthManager auth_mgr(
            "replace-this-dev-secret-before-production", temp_config_.string());
        return auth_mgr.generate_access_token(uid, uid + "-account", "remote-message-device",
                                             "web", 3600);
    }

    int ws_port_ = 0;
    int http_port_ = 0;
    std::filesystem::path temp_config_;
    std::unique_ptr<MessageServerApp> message_server_;
    std::unique_ptr<GatewayServer> gateway_;
};

}  // namespace

TEST_F(RemoteMessageGatewayServerSmokeTest, MessageHttpRoutesUseRemoteMessageServer) {
    start_stack();

    httplib::Client client("127.0.0.1", http_port_);
    client.set_connection_timeout(0, 200000);
    client.set_read_timeout(1, 0);

    const std::string sender = std::string(kUidPrefix) + "sender";
    const std::string receiver = std::string(kUidPrefix) + "receiver";
    const std::string sender_token = make_access_token(sender);
    const std::string receiver_token = make_access_token(receiver);

    httplib::Headers sender_headers = {
        {"Authorization", "Bearer " + sender_token},
    };
    json send_body;
    send_body["receiver_uid"] = receiver;
    send_body["content"] = "remote message gateway send";

    auto send = client.Post(
        "/api/v1/messages/send", sender_headers, send_body.dump(), "application/json");
    ASSERT_TRUE(send) << "HTTP send request failed";
    ASSERT_EQ(send->status, 201) << send->body;
    auto send_json = json::parse(send->body);
    ASSERT_TRUE(send_json.contains("message"));
    EXPECT_EQ(send_json["message"]["sender_uid"], sender);
    EXPECT_EQ(send_json["message"]["receiver_uid"], receiver);
    EXPECT_EQ(send_json["message"]["content"], "remote message gateway send");

    auto history = client.Get(
        "/api/v1/messages/history?peer_uid=" + receiver, sender_headers);
    ASSERT_TRUE(history) << "HTTP history request failed";
    ASSERT_EQ(history->status, 200) << history->body;
    auto history_json = json::parse(history->body);
    ASSERT_TRUE(history_json.contains("messages"));
    ASSERT_GE(history_json["messages"].size(), 1);
    EXPECT_EQ(history_json["messages"][0]["content"], "remote message gateway send");

    httplib::Headers receiver_headers = {
        {"Authorization", "Bearer " + receiver_token},
    };
    auto offline = client.Get("/api/v1/messages/offline", receiver_headers);
    ASSERT_TRUE(offline) << "HTTP offline request failed";
    ASSERT_EQ(offline->status, 200) << offline->body;
    auto offline_json = json::parse(offline->body);
    ASSERT_TRUE(offline_json.contains("messages"));
    ASSERT_GE(offline_json["messages"].size(), 1);
    EXPECT_EQ(offline_json["messages"][0]["sender_uid"], sender);
    EXPECT_EQ(offline_json["messages"][0]["status"],
              static_cast<int>(im::service::message::MessageStatus::DELIVERED));

    auto offline_again = client.Get("/api/v1/messages/offline", receiver_headers);
    ASSERT_TRUE(offline_again) << "HTTP offline retry request failed";
    ASSERT_EQ(offline_again->status, 200) << offline_again->body;
    auto offline_again_json = json::parse(offline_again->body);
    ASSERT_TRUE(offline_again_json.contains("messages"));
    EXPECT_EQ(offline_again_json["messages"].size(), 0);
}
