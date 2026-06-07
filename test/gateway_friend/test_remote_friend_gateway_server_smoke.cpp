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
#include <friend.hpp>
#include <friend_service.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <gateway/gateway_server/gateway_server.hpp>
#include <password_hasher.hpp>
#include <services/friend/friend_server_app.hpp>
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
using im::service::friend_::FriendServerApp;
using im::service::friend_::FriendServerConfig;
using im::service::friend_::FriendStatus;
using im::service::user::PasswordHasher;
using im::service::user::RegisterRequest;
using im::service::user::UserService;

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";
constexpr const char* kAccountPrefix = "remote-friend-gateway-test-";
constexpr int64_t kNowMs = 1719000000000;

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

void cleanup_friend_data() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_friends"
        WHERE "requester_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'remote-friend-gateway-test-%'
            )
           OR "target_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'remote-friend-gateway-test-%'
            ))");
    db.execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'remote-friend-gateway-test-%')");
    t.commit();
}

std::filesystem::path write_temp_config(int ws_port, int http_port, int friend_port) {
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
    config["friend"]["mode"] = "remote";
    config["friend"]["remote_endpoint"] = "127.0.0.1:" + std::to_string(friend_port);
    config["friend"]["timeout_ms"] = 1000;
    config["user"]["mode"] = "local";
    config["message"]["mode"] = "local";
    config["push"]["mode"] = "local";

    auto out_path = std::filesystem::temp_directory_path() /
        ("mychat-remote-friend-gateway-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    std::ofstream output(out_path);
    if (!output) {
        throw std::runtime_error("Failed to write temp Gateway config");
    }
    output << config.dump(2);
    return out_path;
}

class RemoteFriendGatewayServerSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_schema();
        cleanup_friend_data();
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
        friend_server_.reset();
        cleanup_friend_data();
        db_.reset();
        redis_manager().shutdown();
        if (!temp_config_.empty()) {
            std::error_code ec;
            std::filesystem::remove(temp_config_, ec);
        }
    }

    void start_stack() {
        FriendServerConfig friend_config;
        friend_config.listen_address = "127.0.0.1:0";
        friend_config.postgres_connection_string = kConnStr;
        friend_server_ = std::make_unique<FriendServerApp>(friend_config);
        ASSERT_TRUE(friend_server_->start());
        ASSERT_GT(friend_server_->selected_port(), 0);

        ws_port_ = find_free_port();
        http_port_ = find_free_port();
        temp_config_ = write_temp_config(
            ws_port_, http_port_, friend_server_->selected_port());
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
        return auth_mgr.generate_access_token(uid, uid + "-account", "remote-friend-device",
                                             "web", 3600);
    }

    int ws_port_ = 0;
    int http_port_ = 0;
    std::filesystem::path temp_config_;
    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<FriendServerApp> friend_server_;
    std::unique_ptr<GatewayServer> gateway_;
};

}  // namespace

TEST_F(RemoteFriendGatewayServerSmokeTest, FriendHttpRoutesUseRemoteFriendServer) {
    const std::string uid_alice = create_user(std::string(kAccountPrefix) + "alice");
    const std::string uid_bob = create_user(std::string(kAccountPrefix) + "bob");
    ASSERT_FALSE(uid_alice.empty());
    ASSERT_FALSE(uid_bob.empty());

    start_stack();

    httplib::Client client("127.0.0.1", http_port_);
    client.set_connection_timeout(0, 200000);
    client.set_read_timeout(1, 0);

    httplib::Headers alice_headers = {
        {"Authorization", "Bearer " + make_access_token(uid_alice)},
    };
    httplib::Headers bob_headers = {
        {"Authorization", "Bearer " + make_access_token(uid_bob)},
    };

    json send_body;
    send_body["target_uid"] = uid_bob;
    auto send = client.Post(
        "/api/v1/friends/request", alice_headers, send_body.dump(), "application/json");
    ASSERT_TRUE(send) << "HTTP friend request failed";
    ASSERT_EQ(send->status, 201) << send->body;
    EXPECT_EQ(json::parse(send->body)["message"], "Friend request sent");

    auto pending = client.Get("/api/v1/friends/pending", bob_headers);
    ASSERT_TRUE(pending) << "HTTP pending request failed";
    ASSERT_EQ(pending->status, 200) << pending->body;
    auto pending_json = json::parse(pending->body);
    ASSERT_TRUE(pending_json.contains("pending_requests"));
    ASSERT_EQ(pending_json["pending_requests"].size(), 1);
    const uint64_t friend_id = pending_json["pending_requests"][0]["friend_id"];
    EXPECT_EQ(pending_json["pending_requests"][0]["friend_uid"], uid_alice);
    EXPECT_EQ(pending_json["pending_requests"][0]["status"],
              static_cast<int>(FriendStatus::PENDING));

    json respond_body;
    respond_body["friend_id"] = friend_id;
    respond_body["accept"] = true;
    auto respond = client.Post(
        "/api/v1/friends/respond", bob_headers, respond_body.dump(), "application/json");
    ASSERT_TRUE(respond) << "HTTP friend response failed";
    ASSERT_EQ(respond->status, 200) << respond->body;
    EXPECT_EQ(json::parse(respond->body)["message"], "Friend request accepted");

    auto friends = client.Get("/api/v1/friends", alice_headers);
    ASSERT_TRUE(friends) << "HTTP friends list failed";
    ASSERT_EQ(friends->status, 200) << friends->body;
    auto friends_json = json::parse(friends->body);
    ASSERT_TRUE(friends_json.contains("friends"));
    ASSERT_EQ(friends_json["friends"].size(), 1);
    EXPECT_EQ(friends_json["friends"][0]["friend_uid"], uid_bob);
    EXPECT_EQ(friends_json["friends"][0]["nickname"], std::string(kAccountPrefix) + "bob");
    EXPECT_EQ(friends_json["friends"][0]["status"],
              static_cast<int>(FriendStatus::ACCEPTED));
}
