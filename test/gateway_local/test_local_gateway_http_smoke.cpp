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

#include "../support/postgres_schema.hpp"

namespace {

namespace net = boost::asio;
using json = nlohmann::json;
using tcp = net::ip::tcp;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::GatewayServer;

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";
constexpr const char* kAccountPrefix = "local-gateway-smoke-";
constexpr const char* kGroupPrefix = "local-gateway-smoke-";

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

void cleanup_local_smoke_data() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_group_messages"
        WHERE "group_id" IN (
                SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'local-gateway-smoke-%'
            )
           OR "sender_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'local-gateway-smoke-%'
            ))");
    db.execute(R"(DELETE FROM "im_group_members"
        WHERE "group_id" IN (
                SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'local-gateway-smoke-%'
            )
           OR "user_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'local-gateway-smoke-%'
            ))");
    db.execute(R"(DELETE FROM "im_groups"
        WHERE "creator_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'local-gateway-smoke-%'
            )
           OR "name" LIKE 'local-gateway-smoke-%')");
    db.execute(R"(DELETE FROM "im_friends"
        WHERE "requester_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'local-gateway-smoke-%'
            )
           OR "target_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'local-gateway-smoke-%'
            ))");
    db.execute(R"(DELETE FROM "im_messages"
        WHERE "sender_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'local-gateway-smoke-%'
            )
           OR "receiver_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'local-gateway-smoke-%'
            ))");
    db.execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'local-gateway-smoke-%')");
    t.commit();
}

std::filesystem::path write_temp_config(int ws_port, int http_port) {
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
    config["group"]["mode"] = "local";
    config["push"]["mode"] = "local";

    auto out_path = std::filesystem::temp_directory_path() /
        ("mychat-local-gateway-smoke-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    std::ofstream output(out_path);
    if (!output) {
        throw std::runtime_error("Failed to write temp Gateway config");
    }
    output << config.dump(2);
    return out_path;
}

struct TestUser {
    std::string account;
    std::string uid;
    std::string token;
};

class LocalGatewayHttpSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_schema();
        cleanup_local_smoke_data();
        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";
    }

    void TearDown() override {
        if (gateway_) {
            gateway_->stop();
            gateway_.reset();
        }
        cleanup_local_smoke_data();
        redis_manager().shutdown();
        if (!temp_config_.empty()) {
            std::error_code ec;
            std::filesystem::remove(temp_config_, ec);
        }
    }

    void start_gateway() {
        ws_port_ = find_free_port();
        http_port_ = find_free_port();
        temp_config_ = write_temp_config(ws_port_, http_port_);
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

    httplib::Client make_client() const {
        httplib::Client client("127.0.0.1", http_port_);
        client.set_connection_timeout(0, 200000);
        client.set_read_timeout(2, 0);
        return client;
    }

    TestUser register_user(const std::string& suffix, const std::string& nickname) const {
        auto client = make_client();
        json body;
        body["account"] = std::string(kAccountPrefix) + suffix;
        body["password"] = "localPass123";
        body["nickname"] = nickname;
        body["device_id"] = "local-smoke-device-" + suffix;
        body["platform"] = "web";

        auto response = client.Post("/api/v1/auth/register",
                                    body.dump(),
                                    "application/json");
        EXPECT_TRUE(response) << "HTTP register request failed";
        EXPECT_EQ(response->status, 201) << (response ? response->body : "");
        const auto data = json::parse(response->body);
        EXPECT_TRUE(data.contains("access_token"));
        EXPECT_TRUE(data.contains("profile"));
        return TestUser{
            body["account"].get<std::string>(),
            data["profile"]["uid"].get<std::string>(),
            data["access_token"].get<std::string>(),
        };
    }

    httplib::Headers auth_headers(const TestUser& user) const {
        return {{"Authorization", "Bearer " + user.token}};
    }

    int ws_port_ = 0;
    int http_port_ = 0;
    std::filesystem::path temp_config_;
    std::unique_ptr<GatewayServer> gateway_;
};

}  // namespace

TEST_F(LocalGatewayHttpSmokeTest, UnifiedHttpEntryUsesLocalDispatchersForCoreImFlow) {
    start_gateway();

    const TestUser alice = register_user("alice", "Local Alice");
    const TestUser bob = register_user("bob", "Local Bob");
    ASSERT_FALSE(alice.uid.empty());
    ASSERT_FALSE(bob.uid.empty());

    auto client = make_client();

    auto profile = client.Get("/api/v1/auth/info", auth_headers(alice));
    ASSERT_TRUE(profile) << "HTTP profile request failed";
    ASSERT_EQ(profile->status, 200) << profile->body;
    EXPECT_EQ(json::parse(profile->body)["profile"]["account"], alice.account);

    auto search_user = client.Get(
        ("/api/v1/users/search?q=" + bob.account).c_str(),
        auth_headers(alice));
    ASSERT_TRUE(search_user) << "HTTP user search failed";
    ASSERT_EQ(search_user->status, 200) << search_user->body;
    const auto search_user_json = json::parse(search_user->body);
    ASSERT_GE(search_user_json["users"].size(), 1);
    EXPECT_EQ(search_user_json["users"][0]["account"], bob.account);

    json friend_body;
    friend_body["target_uid"] = bob.uid;
    auto friend_request = client.Post("/api/v1/friends/request",
                                      auth_headers(alice),
                                      friend_body.dump(),
                                      "application/json");
    ASSERT_TRUE(friend_request) << "HTTP friend request failed";
    ASSERT_EQ(friend_request->status, 201) << friend_request->body;

    auto pending = client.Get("/api/v1/friends/pending", auth_headers(bob));
    ASSERT_TRUE(pending) << "HTTP pending friend request failed";
    ASSERT_EQ(pending->status, 200) << pending->body;
    const auto pending_json = json::parse(pending->body);
    ASSERT_EQ(pending_json["pending_requests"].size(), 1);
    const uint64_t friend_id =
        pending_json["pending_requests"][0]["friend_id"].get<uint64_t>();

    json respond_body;
    respond_body["friend_id"] = friend_id;
    respond_body["accept"] = true;
    auto respond = client.Post("/api/v1/friends/respond",
                               auth_headers(bob),
                               respond_body.dump(),
                               "application/json");
    ASSERT_TRUE(respond) << "HTTP friend response failed";
    ASSERT_EQ(respond->status, 200) << respond->body;

    auto friends = client.Get("/api/v1/friends", auth_headers(alice));
    ASSERT_TRUE(friends) << "HTTP friend list failed";
    ASSERT_EQ(friends->status, 200) << friends->body;
    const auto friends_json = json::parse(friends->body);
    ASSERT_EQ(friends_json["friends"].size(), 1);
    EXPECT_EQ(friends_json["friends"][0]["friend_uid"], bob.uid);

    json send_body;
    send_body["receiver_uid"] = bob.uid;
    send_body["content"] = "local direct hello";
    auto send = client.Post("/api/v1/messages/send",
                            auth_headers(alice),
                            send_body.dump(),
                            "application/json");
    ASSERT_TRUE(send) << "HTTP direct message send failed";
    ASSERT_EQ(send->status, 201) << send->body;
    const auto send_json = json::parse(send->body);
    EXPECT_EQ(send_json["message"]["sender_uid"], alice.uid);
    EXPECT_EQ(send_json["message"]["receiver_uid"], bob.uid);

    auto history = client.Get(
        ("/api/v1/messages/history?peer_uid=" + bob.uid).c_str(),
        auth_headers(alice));
    ASSERT_TRUE(history) << "HTTP direct message history failed";
    ASSERT_EQ(history->status, 200) << history->body;
    const auto history_json = json::parse(history->body);
    ASSERT_GE(history_json["messages"].size(), 1);
    EXPECT_EQ(history_json["messages"][0]["content"], "local direct hello");

    auto offline = client.Get("/api/v1/messages/offline", auth_headers(bob));
    ASSERT_TRUE(offline) << "HTTP offline pull failed";
    ASSERT_EQ(offline->status, 200) << offline->body;
    const auto offline_json = json::parse(offline->body);
    ASSERT_GE(offline_json["messages"].size(), 1);
    EXPECT_EQ(offline_json["messages"][0]["sender_uid"], alice.uid);

    json group_body;
    group_body["name"] = std::string(kGroupPrefix) + "room";
    auto create_group = client.Post("/api/v1/groups",
                                    auth_headers(alice),
                                    group_body.dump(),
                                    "application/json");
    ASSERT_TRUE(create_group) << "HTTP create group failed";
    ASSERT_EQ(create_group->status, 201) << create_group->body;
    const uint64_t group_id = json::parse(create_group->body)["group_id"].get<uint64_t>();
    ASSERT_GT(group_id, 0u);

    auto search_group = client.Get(
        ("/api/v1/groups/search?q=" + std::string(kGroupPrefix)).c_str(),
        auth_headers(bob));
    ASSERT_TRUE(search_group) << "HTTP group search failed";
    ASSERT_EQ(search_group->status, 200) << search_group->body;
    ASSERT_GE(json::parse(search_group->body)["groups"].size(), 1);

    auto group_info = client.Get(
        ("/api/v1/groups/info?group_id=" + std::to_string(group_id)).c_str(),
        auth_headers(alice));
    ASSERT_TRUE(group_info) << "HTTP group info failed";
    ASSERT_EQ(group_info->status, 200) << group_info->body;
    EXPECT_EQ(json::parse(group_info->body)["group"]["group_id"], group_id);

    json join_body;
    join_body["group_id"] = group_id;
    auto join_group = client.Post("/api/v1/groups/join",
                                  auth_headers(bob),
                                  join_body.dump(),
                                  "application/json");
    ASSERT_TRUE(join_group) << "HTTP join group failed";
    ASSERT_EQ(join_group->status, 200) << join_group->body;

    auto group_list = client.Get("/api/v1/groups", auth_headers(bob));
    ASSERT_TRUE(group_list) << "HTTP group list failed";
    ASSERT_EQ(group_list->status, 200) << group_list->body;
    ASSERT_EQ(json::parse(group_list->body)["groups"].size(), 1);

    auto members = client.Get(
        ("/api/v1/groups/members?group_id=" + std::to_string(group_id)).c_str(),
        auth_headers(alice));
    ASSERT_TRUE(members) << "HTTP group members failed";
    ASSERT_EQ(members->status, 200) << members->body;
    ASSERT_EQ(json::parse(members->body)["members"].size(), 2);

    json group_message_body;
    group_message_body["group_id"] = group_id;
    group_message_body["content"] = "local group hello";
    auto send_group_message = client.Post("/api/v1/groups/messages/send",
                                          auth_headers(bob),
                                          group_message_body.dump(),
                                          "application/json");
    ASSERT_TRUE(send_group_message) << "HTTP group message send failed";
    ASSERT_EQ(send_group_message->status, 201) << send_group_message->body;

    auto group_history = client.Get(
        ("/api/v1/groups/messages/history?group_id=" + std::to_string(group_id)).c_str(),
        auth_headers(alice));
    ASSERT_TRUE(group_history) << "HTTP group history failed";
    ASSERT_EQ(group_history->status, 200) << group_history->body;
    const auto group_history_json = json::parse(group_history->body);
    ASSERT_EQ(group_history_json["messages"].size(), 1);
    EXPECT_EQ(group_history_json["messages"][0]["content"], "local group hello");
}
