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
#include <gateway/auth/multi_platform_auth.hpp>
#include <gateway/gateway_server/gateway_server.hpp>
#include <password_hasher.hpp>
#include <services/group/group_server_app.hpp>
#include <user_service.hpp>

#include "../support/postgres_schema.hpp"

namespace {

namespace net = boost::asio;
using json = nlohmann::json;
using tcp = net::ip::tcp;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::GatewayServer;
using im::gateway::MultiPlatformAuthManager;
using im::service::group::GroupServerApp;
using im::service::group::GroupServerConfig;
using im::service::user::PasswordHasher;
using im::service::user::RegisterRequest;
using im::service::user::UserService;

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";
constexpr const char* kAccountPrefix = "remote-group-gateway-test-";
constexpr const char* kGroupPrefix = "remote-group-gateway-test-";
constexpr int64_t kNowMs = 1722000000000;

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

void cleanup_group_data() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_group_messages"
        WHERE "group_id" IN (
                SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'remote-group-gateway-test-%'
            )
           OR "sender_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'remote-group-gateway-test-%'
            ))");
    db.execute(R"(DELETE FROM "im_group_members"
        WHERE "group_id" IN (
                SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'remote-group-gateway-test-%'
            )
           OR "user_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'remote-group-gateway-test-%'
            ))");
    db.execute(R"(DELETE FROM "im_groups"
        WHERE "creator_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'remote-group-gateway-test-%'
            )
           OR "name" LIKE 'remote-group-gateway-test-%')");
    db.execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'remote-group-gateway-test-%')");
    t.commit();
}

std::filesystem::path write_temp_config(int ws_port, int http_port, int group_port) {
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
    config["user"]["mode"] = "local";
    config["message"]["mode"] = "local";
    config["friend"]["mode"] = "local";
    config["group"]["mode"] = "remote";
    config["group"]["remote_endpoint"] = "127.0.0.1:" + std::to_string(group_port);
    config["group"]["timeout_ms"] = 1000;
    config["push"]["mode"] = "local";

    auto out_path = std::filesystem::temp_directory_path() /
        ("mychat-remote-group-gateway-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    std::ofstream output(out_path);
    if (!output) {
        throw std::runtime_error("Failed to write temp Gateway config");
    }
    output << config.dump(2);
    return out_path;
}

class RemoteGroupGatewayServerSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_schema();
        cleanup_group_data();
        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
    }

    void TearDown() override {
        if (gateway_) {
            gateway_->stop();
            gateway_.reset();
        }
        group_server_.reset();
        cleanup_group_data();
        db_.reset();
        redis_manager().shutdown();
        if (!temp_config_.empty()) {
            std::error_code ec;
            std::filesystem::remove(temp_config_, ec);
        }
    }

    void start_stack() {
        GroupServerConfig group_config;
        group_config.listen_address = "127.0.0.1:0";
        group_config.postgres_connection_string = kConnStr;
        group_server_ = std::make_unique<GroupServerApp>(group_config);
        ASSERT_TRUE(group_server_->start());
        ASSERT_GT(group_server_->selected_port(), 0);

        ws_port_ = find_free_port();
        http_port_ = find_free_port();
        temp_config_ = write_temp_config(
            ws_port_, http_port_, group_server_->selected_port());
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

    std::string create_user(const std::string& account) {
        UserService user_service(db_, std::make_unique<PasswordHasher>());
        RegisterRequest request;
        request.account = account;
        request.password = "remotePass123";
        request.nickname = account;
        request.now_ms = kNowMs;
        auto result = user_service.register_user(request);
        EXPECT_TRUE(result.ok) << "Failed to create " << account << ": " << result.message;
        return result.profile.uid;
    }

    std::string make_access_token(const std::string& uid) {
        MultiPlatformAuthManager auth_mgr(
            "replace-this-dev-secret-before-production", temp_config_.string());
        return auth_mgr.generate_access_token(uid, uid + "-account", "remote-group-device",
                                             "web", 3600);
    }

    int ws_port_ = 0;
    int http_port_ = 0;
    std::filesystem::path temp_config_;
    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<GroupServerApp> group_server_;
    std::unique_ptr<GatewayServer> gateway_;
};

}  // namespace

TEST_F(RemoteGroupGatewayServerSmokeTest, GroupHttpRoutesUseRemoteGroupServer) {
    const std::string owner = create_user(std::string(kAccountPrefix) + "owner");
    const std::string member = create_user(std::string(kAccountPrefix) + "member");
    ASSERT_FALSE(owner.empty());
    ASSERT_FALSE(member.empty());

    start_stack();

    httplib::Client client("127.0.0.1", http_port_);
    client.set_connection_timeout(0, 200000);
    client.set_read_timeout(1, 0);

    httplib::Headers owner_headers = {
        {"Authorization", "Bearer " + make_access_token(owner)},
    };
    httplib::Headers member_headers = {
        {"Authorization", "Bearer " + make_access_token(member)},
    };

    json create_body;
    create_body["name"] = std::string(kGroupPrefix) + "room";
    auto create = client.Post(
        "/api/v1/groups", owner_headers, create_body.dump(), "application/json");
    ASSERT_TRUE(create) << "HTTP create group failed";
    ASSERT_EQ(create->status, 201) << create->body;
    auto create_json = json::parse(create->body);
    ASSERT_TRUE(create_json.contains("group_id"));
    const uint64_t group_id = create_json["group_id"].get<uint64_t>();
    EXPECT_GT(group_id, 0u);

    json join_body;
    join_body["group_id"] = group_id;
    auto join = client.Post(
        "/api/v1/groups/join", member_headers, join_body.dump(), "application/json");
    ASSERT_TRUE(join) << "HTTP join group failed";
    ASSERT_EQ(join->status, 200) << join->body;

    auto groups = client.Get("/api/v1/groups", member_headers);
    ASSERT_TRUE(groups) << "HTTP list groups failed";
    ASSERT_EQ(groups->status, 200) << groups->body;
    auto groups_json = json::parse(groups->body);
    ASSERT_TRUE(groups_json.contains("groups"));
    ASSERT_EQ(groups_json["groups"].size(), 1);
    EXPECT_EQ(groups_json["groups"][0]["group_id"], group_id);
    EXPECT_EQ(groups_json["groups"][0]["name"], create_body["name"]);

    auto members = client.Get(
        ("/api/v1/groups/members?group_id=" + std::to_string(group_id)).c_str(),
        owner_headers);
    ASSERT_TRUE(members) << "HTTP list members failed";
    ASSERT_EQ(members->status, 200) << members->body;
    auto members_json = json::parse(members->body);
    ASSERT_TRUE(members_json.contains("members"));
    ASSERT_EQ(members_json["members"].size(), 2);

    json send_body;
    send_body["group_id"] = group_id;
    send_body["content"] = "remote group hello";
    auto send = client.Post(
        "/api/v1/groups/messages/send", member_headers, send_body.dump(),
        "application/json");
    ASSERT_TRUE(send) << "HTTP send group message failed";
    ASSERT_EQ(send->status, 201) << send->body;
    auto send_json = json::parse(send->body);
    ASSERT_TRUE(send_json.contains("msg_id"));
    EXPECT_GT(send_json["msg_id"].get<uint64_t>(), 0u);

    auto history = client.Get(
        ("/api/v1/groups/messages/history?group_id=" + std::to_string(group_id)).c_str(),
        owner_headers);
    ASSERT_TRUE(history) << "HTTP group history failed";
    ASSERT_EQ(history->status, 200) << history->body;
    auto history_json = json::parse(history->body);
    ASSERT_TRUE(history_json.contains("messages"));
    ASSERT_EQ(history_json["messages"].size(), 1);
    EXPECT_EQ(history_json["messages"][0]["content"], "remote group hello");
    EXPECT_EQ(history_json["messages"][0]["sender_uid"], member);
}
