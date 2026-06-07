#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <friend.hpp>
#include <friend-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/http/friend_client.hpp>
#include <gateway/http/friend_http_controller.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <friend_service.hpp>
#include <user_service.hpp>
#include <password_hasher.hpp>
#include <utils/log_manager.hpp>

#include "../support/postgres_schema.hpp"

// Route registration free function (defined in gateway_server.cpp)
void register_friend_http_routes_on_server(
    httplib::Server& server,
    im::gateway::FriendHttpController& controller);

namespace {

using json = nlohmann::json;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::FriendHttpController;
using im::gateway::LocalFriendClient;
using im::gateway::MultiPlatformAuthManager;
using im::service::friend_::FriendService;
using im::service::friend_::FriendInfoDTO;
using im::service::friend_::FriendStatus;
using im::service::user::UserService;
using im::service::user::PasswordHasher;
using im::service::user::RegisterRequest;
using im::utils::LogManager;

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1719000000000;

RedisConfig test_redis_config() {
    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "mychat-dev-pass";
    config.db = 15;
    config.connect_timeout = 1000;
    config.socket_timeout = 1000;
    return config;
}

std::string config_path() {
    return (std::filesystem::path(MYCHAT_SOURCE_DIR) / "config/dev.json").string();
}

class GatewayFriendHttpTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("auth_mgr");
        LogManager::SetLogToConsole("redis_manager");
        LogManager::SetLogToConsole("friend_http_controller");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTables();
        CleanupTestData();

        auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(
            "test_secret_key_for_gateway_friend_http_tests", config_path());
        auto hasher = std::make_unique<PasswordHasher>();
        auto user_svc = std::make_shared<UserService>(db_, std::move(hasher));
        friend_svc_ = std::make_shared<FriendService>(db_, user_svc);
        friend_client_ = std::make_shared<LocalFriendClient>(friend_svc_);
        controller_ = std::make_unique<FriendHttpController>(friend_client_, auth_mgr_);
    }

    void TearDown() override {
        CleanupTestData();
        controller_.reset();
        auth_mgr_.reset();
        redis_manager().shutdown();
    }

    static void EnsureTables() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void CleanupTestData() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_friends"
            WHERE "requester_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'friend-http-test-%'
                )
               OR "target_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'friend-http-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'friend-http-test-%')");
        t.commit();
    }

    std::string make_token(const std::string& uid) {
        return auth_mgr_->generate_access_token(uid, uid + "-acct", "test-device", "web", 3600);
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<FriendService> friend_svc_;
    std::shared_ptr<LocalFriendClient> friend_client_;
    std::unique_ptr<FriendHttpController> controller_;
};

json parse_body(const httplib::Response& res) {
    return json::parse(res.body);
}

// ── Auth / token tests ─────────────────────────────────────────────────

TEST_F(GatewayFriendHttpTest, SendRequestMissingTokenReturns401) {
    httplib::Request req;
    req.method = "POST";
    json body;
    body["target_uid"] = "friend-http-test-someone";
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send_request(req, res);

    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, SendRequestInvalidTokenReturns401) {
    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer definitely-invalid-token");
    json body;
    body["target_uid"] = "friend-http-test-someone";
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send_request(req, res);

    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, SendRequestInvalidJsonReturns400) {
    std::string token = make_token("friend-http-test-user");

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    req.body = "not-valid-json";

    httplib::Response res;
    controller_->handle_send_request(req, res);

    EXPECT_EQ(res.status, 400) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, SendRequestMissingTargetUidReturns400) {
    std::string token = make_token("friend-http-test-user");

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    json body;
    body["some_other_field"] = "value";
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send_request(req, res);

    EXPECT_EQ(res.status, 400) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, RespondMissingFriendIdReturns400) {
    std::string token = make_token("friend-http-test-user");

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    json body;
    body["accept"] = true;
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_respond_request(req, res);

    EXPECT_EQ(res.status, 400) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, ListFriendsMissingTokenReturns401) {
    httplib::Request req;
    req.method = "GET";

    httplib::Response res;
    controller_->handle_list_friends(req, res);

    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, PendingRequestsMissingTokenReturns401) {
    httplib::Request req;
    req.method = "GET";

    httplib::Response res;
    controller_->handle_pending_requests(req, res);

    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

// ── Happy path tests ────────────────────────────────────────────────────

TEST_F(GatewayFriendHttpTest, SendAndRespondHappyPath) {
    // Create two real users in PostgreSQL (needed for friend_svc nickname resolution)
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    auto create = [&](const std::string& acct) -> std::string {
        RegisterRequest r;
        r.account = acct;
        r.password = "pass";
        r.nickname = acct;
        r.now_ms = kNowMs;
        auto res = user_svc->register_user(r);
        EXPECT_TRUE(res.ok) << "Failed to create " << acct;
        return res.profile.uid;
    };
    std::string uid_alice = create("friend-http-test-alice");
    std::string uid_bob = create("friend-http-test-bob");
    ASSERT_FALSE(uid_alice.empty());
    ASSERT_FALSE(uid_bob.empty());

    std::string alice_token = make_token(uid_alice);
    std::string bob_token = make_token(uid_bob);

    // 1) Alice sends a friend request to Bob
    {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + alice_token);
        json body;
        body["target_uid"] = uid_bob;
        req.body = body.dump();

        httplib::Response res;
        controller_->handle_send_request(req, res);

        EXPECT_EQ(res.status, 201) << "Body: " << res.body;
        json j = parse_body(res);
        EXPECT_EQ(j["message"], "Friend request sent");
    }

    // 2) Bob responds (accept)
    {
        // Get the friend_id via FriendService directly
        auto pending = friend_svc_->get_pending_requests(uid_bob);
        ASSERT_EQ(pending.size(), 1);
        uint64_t friend_id = pending[0].friend_id;

        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + bob_token);
        json body;
        body["friend_id"] = friend_id;
        body["accept"] = true;
        req.body = body.dump();

        httplib::Response res;
        controller_->handle_respond_request(req, res);

        EXPECT_EQ(res.status, 200) << "Body: " << res.body;
        json j = parse_body(res);
        EXPECT_EQ(j["message"], "Friend request accepted");
    }

    // 3) List friends for Alice — Bob should appear
    {
        httplib::Request req;
        req.method = "GET";
        req.set_header("Authorization", "Bearer " + alice_token);

        httplib::Response res;
        controller_->handle_list_friends(req, res);

        EXPECT_EQ(res.status, 200) << "Body: " << res.body;
        json j = parse_body(res);
        ASSERT_TRUE(j.contains("friends"));
        ASSERT_EQ(j["friends"].size(), 1);
        EXPECT_EQ(j["friends"][0]["friend_uid"], uid_bob);
        EXPECT_EQ(j["friends"][0]["status"], static_cast<int>(FriendStatus::ACCEPTED));
        EXPECT_EQ(j["friends"][0]["nickname"], "friend-http-test-bob");
    }

    // 4) List friends for Bob — Alice should appear
    {
        httplib::Request req;
        req.method = "GET";
        req.set_header("Authorization", "Bearer " + bob_token);

        httplib::Response res;
        controller_->handle_list_friends(req, res);

        EXPECT_EQ(res.status, 200) << "Body: " << res.body;
        json j = parse_body(res);
        ASSERT_TRUE(j.contains("friends"));
        ASSERT_EQ(j["friends"].size(), 1);
        EXPECT_EQ(j["friends"][0]["friend_uid"], uid_alice);
        EXPECT_EQ(j["friends"][0]["status"], static_cast<int>(FriendStatus::ACCEPTED));
    }
}

TEST_F(GatewayFriendHttpTest, SendRequestSelfRequestRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    RegisterRequest r;
    r.account = "friend-http-test-self";
    r.password = "pass";
    r.now_ms = kNowMs;
    auto reg = user_svc->register_user(r);
    ASSERT_TRUE(reg.ok);
    std::string uid = reg.profile.uid;
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    json body;
    body["target_uid"] = uid;
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send_request(req, res);

    EXPECT_EQ(res.status, 400) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, SendRequestDuplicateReturns409) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    auto create = [&](const std::string& acct) -> std::string {
        RegisterRequest r;
        r.account = acct;
        r.password = "pass";
        r.nickname = acct;
        r.now_ms = kNowMs;
        auto res = user_svc->register_user(r);
        EXPECT_TRUE(res.ok);
        return res.profile.uid;
    };
    std::string uid_a = create("friend-http-test-dup-a");
    std::string uid_b = create("friend-http-test-dup-b");
    std::string token_a = make_token(uid_a);

    auto send_req = [&]() {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + token_a);
        json body;
        body["target_uid"] = uid_b;
        req.body = body.dump();
        httplib::Response res;
        controller_->handle_send_request(req, res);
        return res;
    };

    auto first = send_req();
    EXPECT_EQ(first.status, 201);

    auto second = send_req();
    EXPECT_EQ(second.status, 409) << "Body: " << second.body;
}

TEST_F(GatewayFriendHttpTest, SendRequestTargetNotFoundReturns404) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    RegisterRequest r;
    r.account = "friend-http-test-tnf";
    r.password = "pass";
    r.now_ms = kNowMs;
    auto reg = user_svc->register_user(r);
    ASSERT_TRUE(reg.ok);
    std::string token = make_token(reg.profile.uid);

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    json body;
    body["target_uid"] = "nonexistent-uid-99999";
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send_request(req, res);

    EXPECT_EQ(res.status, 404) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, RespondOnlyForbiddenForNonTarget) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    auto create = [&](const std::string& acct) -> std::string {
        RegisterRequest r;
        r.account = acct;
        r.password = "pass";
        r.nickname = acct;
        r.now_ms = kNowMs;
        auto res = user_svc->register_user(r);
        EXPECT_TRUE(res.ok);
        return res.profile.uid;
    };
    std::string uid_a = create("friend-http-test-forbid-a");
    std::string uid_b = create("friend-http-test-forbid-b");
    std::string uid_c = create("friend-http-test-forbid-c");
    std::string token_a = make_token(uid_a);
    std::string token_c = make_token(uid_c);

    // A -> B
    {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + token_a);
        json body;
        body["target_uid"] = uid_b;
        req.body = body.dump();
        httplib::Response res;
        controller_->handle_send_request(req, res);
        EXPECT_EQ(res.status, 201);
    }

    // Get friend_id
    auto pending = friend_svc_->get_pending_requests(uid_b);
    ASSERT_EQ(pending.size(), 1);
    uint64_t friend_id = pending[0].friend_id;

    // C tries to respond (should be forbidden)
    {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + token_c);
        json body;
        body["friend_id"] = friend_id;
        body["accept"] = true;
        req.body = body.dump();
        httplib::Response res;
        controller_->handle_respond_request(req, res);

        EXPECT_EQ(res.status, 403) << "Body: " << res.body;
    }
}

TEST_F(GatewayFriendHttpTest, RespondNotFoundReturns404) {
    std::string token = make_token("friend-http-test-nf");

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    json body;
    body["friend_id"] = 99999999;
    body["accept"] = true;
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_respond_request(req, res);

    EXPECT_EQ(res.status, 404) << "Body: " << res.body;
}

TEST_F(GatewayFriendHttpTest, PendingRequestsList) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    auto create = [&](const std::string& acct) -> std::string {
        RegisterRequest r;
        r.account = acct;
        r.password = "pass";
        r.nickname = acct;
        r.now_ms = kNowMs;
        auto res = user_svc->register_user(r);
        EXPECT_TRUE(res.ok);
        return res.profile.uid;
    };
    std::string uid_a = create("friend-http-test-pend-alice");
    std::string uid_b = create("friend-http-test-pend-bob");
    std::string token_a = make_token(uid_a);
    std::string token_b = make_token(uid_b);

    // A -> B
    {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + token_a);
        json body;
        body["target_uid"] = uid_b;
        req.body = body.dump();
        httplib::Response res;
        controller_->handle_send_request(req, res);
        EXPECT_EQ(res.status, 201);
    }

    // B sees pending
    {
        httplib::Request req;
        req.method = "GET";
        req.set_header("Authorization", "Bearer " + token_b);

        httplib::Response res;
        controller_->handle_pending_requests(req, res);

        EXPECT_EQ(res.status, 200) << "Body: " << res.body;
        json j = parse_body(res);
        ASSERT_TRUE(j.contains("pending_requests"));
        ASSERT_EQ(j["pending_requests"].size(), 1);
        EXPECT_EQ(j["pending_requests"][0]["friend_uid"], uid_a);
        EXPECT_EQ(j["pending_requests"][0]["status"], static_cast<int>(FriendStatus::PENDING));
    }

    // A does NOT see pending (A is the requester)
    {
        httplib::Request req;
        req.method = "GET";
        req.set_header("Authorization", "Bearer " + token_a);

        httplib::Response res;
        controller_->handle_pending_requests(req, res);

        EXPECT_EQ(res.status, 200) << "Body: " << res.body;
        json j = parse_body(res);
        ASSERT_TRUE(j.contains("pending_requests"));
        EXPECT_EQ(j["pending_requests"].size(), 0);
    }
}

TEST_F(GatewayFriendHttpTest, FriendsListReturnsEmptyForNoFriends) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    RegisterRequest r;
    r.account = "friend-http-test-empty-list";
    r.password = "pass";
    r.now_ms = kNowMs;
    auto reg = user_svc->register_user(r);
    ASSERT_TRUE(reg.ok);
    std::string token = make_token(reg.profile.uid);

    httplib::Request req;
    req.method = "GET";
    req.set_header("Authorization", "Bearer " + token);

    httplib::Response res;
    controller_->handle_list_friends(req, res);

    EXPECT_EQ(res.status, 200) << "Body: " << res.body;
    json j = parse_body(res);
    ASSERT_TRUE(j.contains("friends"));
    EXPECT_EQ(j["friends"].size(), 0);
}

// ── Route registration test ─────────────────────────────────────────────

TEST_F(GatewayFriendHttpTest, RoutesAreRegisteredAndHandleRequests) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    auto create = [&](const std::string& acct) -> std::string {
        RegisterRequest r;
        r.account = acct;
        r.password = "pass";
        r.nickname = acct;
        r.now_ms = kNowMs;
        auto res = user_svc->register_user(r);
        EXPECT_TRUE(res.ok);
        return res.profile.uid;
    };

    std::string uid_a = create("friend-http-test-route-a");
    std::string uid_b = create("friend-http-test-route-b");
    std::string token_a = make_token(uid_a);
    std::string token_b = make_token(uid_b);

    httplib::Server svr;
    register_friend_http_routes_on_server(svr, *controller_);
    svr.Get(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 599;
        res.set_content("catch-all", "text/plain");
    });
    svr.Post(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 599;
        res.set_content("catch-all", "text/plain");
    });

    int test_port = 18569;
    std::thread server_thread([&]() { svr.listen("127.0.0.1", test_port); });
    struct ServerGuard {
        httplib::Server& server;
        std::thread& thread;
        ~ServerGuard() {
            server.stop();
            if (thread.joinable()) {
                thread.join();
            }
        }
    } server_guard{svr, server_thread};
    svr.wait_until_ready();

    httplib::Client cli("127.0.0.1", test_port);

    httplib::Headers auth_header = {{"Authorization", "Bearer " + token_a}};

    // 1) POST /api/v1/friends/request (happy path)
    json send_body;
    send_body["target_uid"] = uid_b;
    auto send_res = cli.Post("/api/v1/friends/request", auth_header,
                             send_body.dump(), "application/json");
    ASSERT_TRUE(send_res) << "HTTP request failed";
    EXPECT_EQ(send_res->status, 201);

    // 2) POST /api/v1/friends/request without auth → 401
    auto unauth_send = cli.Post("/api/v1/friends/request",
                                send_body.dump(), "application/json");
    ASSERT_TRUE(unauth_send);
    EXPECT_EQ(unauth_send->status, 401);

    // 3) GET /api/v1/friends/pending (Bob sees pending)
    {
        httplib::Headers bob_auth = {{"Authorization", "Bearer " + token_b}};
        auto pending_res = cli.Get("/api/v1/friends/pending", bob_auth);
        ASSERT_TRUE(pending_res);
        EXPECT_EQ(pending_res->status, 200);
        json j = json::parse(pending_res->body);
        ASSERT_TRUE(j.contains("pending_requests"));
        EXPECT_GE(j["pending_requests"].size(), 1);
    }

    // 4) GET /api/v1/friends/pending without auth → 401
    auto unauth_pending = cli.Get("/api/v1/friends/pending");
    ASSERT_TRUE(unauth_pending);
    EXPECT_EQ(unauth_pending->status, 401);

    // 5) GET /api/v1/friends without auth → 401
    auto unauth_list = cli.Get("/api/v1/friends");
    ASSERT_TRUE(unauth_list);
    EXPECT_EQ(unauth_list->status, 401);

    // 6) POST /api/v1/friends/respond without auth → 401
    json respond_body;
    respond_body["friend_id"] = 1;
    respond_body["accept"] = true;
    auto unauth_respond = cli.Post("/api/v1/friends/respond",
                                   respond_body.dump(), "application/json");
    ASSERT_TRUE(unauth_respond);
    EXPECT_EQ(unauth_respond->status, 401);
}

} // anonymous namespace
