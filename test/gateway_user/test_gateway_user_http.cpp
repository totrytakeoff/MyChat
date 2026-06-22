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

#include <user.hpp>
#include <user-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/http/user_http_controller.hpp>
#include <gateway/http/user_client.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <user_service.hpp>
#include <password_hasher.hpp>
#include <utils/log_manager.hpp>

#include "../support/postgres_schema.hpp"

// Route registration free function (defined in gateway_server.cpp)
void register_user_http_routes_on_server(httplib::Server& server,
                                         im::gateway::UserHttpController& controller);

namespace {

using json = nlohmann::json;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::UserHttpController;
using im::service::user::PasswordHasher;
using im::service::user::UserService;
using im::utils::LogManager;

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

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

class GatewayUserHttpTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("auth_mgr");
        LogManager::SetLogToConsole("redis_manager");
        LogManager::SetLogToConsole("user_http_controller");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        CleanupTestData();

        auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(
            "test_secret_key_for_gateway_user_http_tests", config_path());
        auto hasher = std::make_unique<PasswordHasher>();
        user_service_ = std::make_shared<UserService>(db_, std::move(hasher));
        user_client_ = std::make_shared<im::gateway::LocalUserClient>(user_service_);
        controller_ = std::make_unique<UserHttpController>(user_client_, auth_mgr_);
    }

    void TearDown() override {
        CleanupTestData();
        controller_.reset();
        auth_mgr_.reset();
        redis_manager().shutdown();
    }

    static void EnsureTable() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void CleanupTestData() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'task6-test-%')");
        t.commit();
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<UserService> user_service_;
    std::shared_ptr<im::gateway::UserClient> user_client_;
    std::unique_ptr<UserHttpController> controller_;
};

// Helper: parse JSON body from a response
json parse_body(const im::gateway::HttpResponse& res) {
    return json::parse(res.body);
}

// Helper: register a user through the controller, return the response
im::gateway::HttpResponse do_register(UserHttpController& ctrl,
                              const std::string& account,
                              const std::string& password,
                              const std::string& nickname = "") {
    im::gateway::HttpRequest req;
    json body;
    body["account"] = account;
    body["password"] = password;
    if (!nickname.empty()) body["nickname"] = nickname;
    req.body = body.dump();
    req.method = "POST";

    im::gateway::HttpResponse res;
    ctrl.handle_register(req, res);
    return res;
}

// Helper: login through the controller
im::gateway::HttpResponse do_login(UserHttpController& ctrl,
                           const std::string& account,
                           const std::string& password) {
    im::gateway::HttpRequest req;
    json body;
    body["account"] = account;
    body["password"] = password;
    req.body = body.dump();
    req.method = "POST";

    im::gateway::HttpResponse res;
    ctrl.handle_login(req, res);
    return res;
}

TEST_F(GatewayUserHttpTest, RegisterReturns201WithTokens) {
    auto res = do_register(*controller_, "task6-test-reg-201", "pass123");

    EXPECT_EQ(res.status, 201) << "Body: " << res.body;

    json j = parse_body(res);
    ASSERT_TRUE(j.contains("access_token"));
    ASSERT_TRUE(j.contains("refresh_token"));
    ASSERT_TRUE(j.contains("profile"));
    EXPECT_EQ(j["profile"]["account"], "task6-test-reg-201");
    EXPECT_FALSE(j["profile"]["uid"].get<std::string>().empty());
}

TEST_F(GatewayUserHttpTest, RegisterDuplicateReturns409) {
    auto first = do_register(*controller_, "task6-test-dup", "pass456");
    EXPECT_EQ(first.status, 201);

    auto second = do_register(*controller_, "task6-test-dup", "pass456");
    EXPECT_EQ(second.status, 409) << "Body: " << second.body;
}

TEST_F(GatewayUserHttpTest, RegisterMissingFieldsReturns400) {
    im::gateway::HttpRequest req;
    json body;
    body["account"] = "task6-test-missing";
    req.body = body.dump();
    req.method = "POST";

    im::gateway::HttpResponse res;
    controller_->handle_register(req, res);
    EXPECT_EQ(res.status, 400) << "Body: " << res.body;
}

TEST_F(GatewayUserHttpTest, LoginReturns200WithTokens) {
    auto reg = do_register(*controller_, "task6-test-login-200", "myPass");
    ASSERT_EQ(reg.status, 201);

    auto res = do_login(*controller_, "task6-test-login-200", "myPass");
    EXPECT_EQ(res.status, 200) << "Body: " << res.body;

    json j = parse_body(res);
    ASSERT_TRUE(j.contains("access_token"));
    ASSERT_TRUE(j.contains("refresh_token"));
    ASSERT_TRUE(j.contains("profile"));
    EXPECT_EQ(j["profile"]["account"], "task6-test-login-200");
}

TEST_F(GatewayUserHttpTest, LoginWrongPasswordReturns401) {
    auto reg = do_register(*controller_, "task6-test-login-401", "correctPass");
    ASSERT_EQ(reg.status, 201);

    auto res = do_login(*controller_, "task6-test-login-401", "wrongPass");
    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayUserHttpTest, ProfileWithValidTokenReturns200) {
    auto reg = do_register(*controller_, "task6-test-profile-200", "pass789");
    ASSERT_EQ(reg.status, 201);
    json reg_j = parse_body(reg);
    std::string access_token = reg_j["access_token"];

    im::gateway::HttpRequest req;
    req.method = "GET";
    req.set_header("Authorization", "Bearer " + access_token);

    im::gateway::HttpResponse res;
    controller_->handle_profile(req, res);
    EXPECT_EQ(res.status, 200) << "Body: " << res.body;

    json j = parse_body(res);
    ASSERT_TRUE(j.contains("profile"));
    EXPECT_EQ(j["profile"]["account"], "task6-test-profile-200");
}

TEST_F(GatewayUserHttpTest, ProfileMissingTokenReturns401) {
    im::gateway::HttpRequest req;
    req.method = "GET";

    im::gateway::HttpResponse res;
    controller_->handle_profile(req, res);
    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayUserHttpTest, ProfileInvalidTokenReturns401) {
    im::gateway::HttpRequest req;
    req.method = "GET";
    req.set_header("Authorization", "Bearer definitely-invalid-token");

    im::gateway::HttpResponse res;
    controller_->handle_profile(req, res);
    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayUserHttpTest, ProfileNonExistentUidReturns404) {
    im::gateway::HttpRequest req;
    req.method = "GET";
    auto token = auth_mgr_->generate_access_token(
        "task6-test-missing-uid", "missing-account", "device-web", "web", 60);
    req.set_header("Authorization", "Bearer " + token);

    im::gateway::HttpResponse res;
    controller_->handle_profile(req, res);
    EXPECT_EQ(res.status, 404) << "Body: " << res.body;
}

TEST_F(GatewayUserHttpTest, UpdateProfileRequiresAuthAndPersistsEditableFields) {
    auto reg = do_register(*controller_, "task6-test-profile-update", "pass789", "Before");
    ASSERT_EQ(reg.status, 201);
    json reg_j = parse_body(reg);
    std::string access_token = reg_j["access_token"];

    im::gateway::HttpRequest missing_auth_req;
    missing_auth_req.method = "POST";
    missing_auth_req.body = R"({"nickname":"NoAuth"})";
    im::gateway::HttpResponse missing_auth_res;
    controller_->handle_update_profile(missing_auth_req, missing_auth_res);
    EXPECT_EQ(missing_auth_res.status, 401) << "Body: " << missing_auth_res.body;

    im::gateway::HttpRequest update_req;
    update_req.method = "POST";
    update_req.set_header("Authorization", "Bearer " + access_token);
    json update_body;
    update_body["nickname"] = "After";
    update_body["avatar"] = "https://example.test/avatar.png";
    update_body["gender"] = 2;
    update_body["signature"] = "profile updated";
    update_req.body = update_body.dump();

    im::gateway::HttpResponse update_res;
    controller_->handle_update_profile(update_req, update_res);
    ASSERT_EQ(update_res.status, 200) << "Body: " << update_res.body;
    json update_j = parse_body(update_res);
    EXPECT_EQ(update_j["profile"]["account"], "task6-test-profile-update");
    EXPECT_EQ(update_j["profile"]["nickname"], "After");
    EXPECT_EQ(update_j["profile"]["avatar"], "https://example.test/avatar.png");
    EXPECT_EQ(update_j["profile"]["gender"], 2);
    EXPECT_EQ(update_j["profile"]["signature"], "profile updated");

    im::gateway::HttpRequest profile_req;
    profile_req.method = "GET";
    profile_req.set_header("Authorization", "Bearer " + access_token);
    im::gateway::HttpResponse profile_res;
    controller_->handle_profile(profile_req, profile_res);
    ASSERT_EQ(profile_res.status, 200) << "Body: " << profile_res.body;
    json profile_j = parse_body(profile_res);
    EXPECT_EQ(profile_j["profile"]["nickname"], "After");
    EXPECT_EQ(profile_j["profile"]["signature"], "profile updated");
}

TEST_F(GatewayUserHttpTest, SearchUserFindsByUidAndAccount) {
    auto alice = do_register(*controller_, "task6-test-search-alice", "pass789", "Alice");
    ASSERT_EQ(alice.status, 201);
    auto bob = do_register(*controller_, "task6-test-search-bob", "pass789", "Bob");
    ASSERT_EQ(bob.status, 201);

    json alice_j = parse_body(alice);
    json bob_j = parse_body(bob);
    std::string access_token = alice_j["access_token"];
    std::string bob_uid = bob_j["profile"]["uid"];

    im::gateway::HttpRequest by_uid_req;
    by_uid_req.method = "GET";
    by_uid_req.set_header("Authorization", "Bearer " + access_token);
    by_uid_req.params.emplace("q", bob_uid);

    im::gateway::HttpResponse by_uid_res;
    controller_->handle_search_user(by_uid_req, by_uid_res);
    ASSERT_EQ(by_uid_res.status, 200) << "Body: " << by_uid_res.body;
    auto by_uid_j = parse_body(by_uid_res);
    EXPECT_EQ(by_uid_j["profile"]["uid"], bob_uid);
    EXPECT_EQ(by_uid_j["profile"]["account"], "task6-test-search-bob");

    im::gateway::HttpRequest by_account_req;
    by_account_req.method = "GET";
    by_account_req.set_header("Authorization", "Bearer " + access_token);
    by_account_req.params.emplace("q", "task6-test-search-bob");

    im::gateway::HttpResponse by_account_res;
    controller_->handle_search_user(by_account_req, by_account_res);
    ASSERT_EQ(by_account_res.status, 200) << "Body: " << by_account_res.body;
    auto by_account_j = parse_body(by_account_res);
    EXPECT_EQ(by_account_j["profile"]["uid"], bob_uid);
    EXPECT_EQ(by_account_j["profile"]["nickname"], "Bob");
    ASSERT_TRUE(by_account_j.contains("users"));
    ASSERT_GE(by_account_j["users"].size(), 1);
}

TEST_F(GatewayUserHttpTest, SearchUserFindsByNicknameAndReturnsList) {
    auto alice = do_register(*controller_, "task6-test-search-nick-a", "pass789", "小明一号");
    ASSERT_EQ(alice.status, 201);
    auto bob = do_register(*controller_, "task6-test-search-nick-b", "pass789", "小明二号");
    ASSERT_EQ(bob.status, 201);

    json alice_j = parse_body(alice);
    std::string access_token = alice_j["access_token"];

    im::gateway::HttpRequest req;
    req.method = "GET";
    req.set_header("Authorization", "Bearer " + access_token);
    req.params.emplace("q", "小明");

    im::gateway::HttpResponse res;
    controller_->handle_search_user(req, res);

    ASSERT_EQ(res.status, 200) << "Body: " << res.body;
    auto body = parse_body(res);
    ASSERT_TRUE(body.contains("users"));
    EXPECT_GE(body["users"].size(), 2);
}

TEST_F(GatewayUserHttpTest, SearchUserRejectsMissingTokenOrUnknownQuery) {
    auto reg = do_register(*controller_, "task6-test-search-auth", "pass789");
    ASSERT_EQ(reg.status, 201);
    std::string access_token = parse_body(reg)["access_token"];

    im::gateway::HttpRequest missing_token;
    missing_token.method = "GET";
    missing_token.params.emplace("q", "task6-test-search-auth");

    im::gateway::HttpResponse missing_token_res;
    controller_->handle_search_user(missing_token, missing_token_res);
    EXPECT_EQ(missing_token_res.status, 401) << "Body: " << missing_token_res.body;

    im::gateway::HttpRequest unknown;
    unknown.method = "GET";
    unknown.set_header("Authorization", "Bearer " + access_token);
    unknown.params.emplace("q", "task6-test-search-missing");

    im::gateway::HttpResponse unknown_res;
    controller_->handle_search_user(unknown, unknown_res);
    EXPECT_EQ(unknown_res.status, 200) << "Body: " << unknown_res.body;
    EXPECT_TRUE(parse_body(unknown_res)["users"].empty());
}

TEST_F(GatewayUserHttpTest, RoutesAreRegisteredAndHandleRequests) {
    httplib::Server svr;
    register_user_http_routes_on_server(svr, *controller_);
    svr.Get(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 599;
        res.set_content("catch-all", "text/plain");
    });
    svr.Post(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 599;
        res.set_content("catch-all", "text/plain");
    });

    int test_port = 18567;
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

    // 1) Register
    json reg_body;
    reg_body["account"] = "task6-route-reg";
    reg_body["password"] = "routePass";
    auto reg_res = cli.Post("/api/v1/auth/register", reg_body.dump(), "application/json");
    ASSERT_TRUE(reg_res) << "HTTP request failed";
    EXPECT_EQ(reg_res->status, 201);
    json reg_j = json::parse(reg_res->body);
    ASSERT_TRUE(reg_j.contains("access_token"));
    std::string token = reg_j["access_token"];
    ASSERT_FALSE(token.empty());

    // 2) Login
    json login_body;
    login_body["account"] = "task6-route-reg";
    login_body["password"] = "routePass";
    auto login_res = cli.Post("/api/v1/auth/login", login_body.dump(), "application/json");
    ASSERT_TRUE(login_res);
    EXPECT_EQ(login_res->status, 200);

    // 3) Profile with valid token
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto profile_res = cli.Get("/api/v1/auth/info", headers);
    ASSERT_TRUE(profile_res);
    EXPECT_EQ(profile_res->status, 200);
    json profile_j = json::parse(profile_res->body);
    ASSERT_TRUE(profile_j.contains("profile"));
    EXPECT_EQ(profile_j["profile"]["account"], "task6-route-reg");

    // 4) Update profile with valid token
    json update_body;
    update_body["nickname"] = "Route Updated";
    auto update_res = cli.Post("/api/v1/auth/profile", headers, update_body.dump(), "application/json");
    ASSERT_TRUE(update_res);
    EXPECT_EQ(update_res->status, 200);
    json update_j = json::parse(update_res->body);
    EXPECT_EQ(update_j["profile"]["nickname"], "Route Updated");

    // 5) Search user by account with valid token
    auto search_res = cli.Get("/api/v1/users/search?q=task6-route-reg", headers);
    ASSERT_TRUE(search_res);
    EXPECT_EQ(search_res->status, 200);
    json search_j = json::parse(search_res->body);
    ASSERT_TRUE(search_j.contains("profile"));
    EXPECT_EQ(search_j["profile"]["account"], "task6-route-reg");

    // 6) Profile without token -> 401
    auto unauth_res = cli.Get("/api/v1/auth/info");
    ASSERT_TRUE(unauth_res);
    EXPECT_EQ(unauth_res->status, 401);

    // 7) Duplicate register -> 409
    auto dup_res = cli.Post("/api/v1/auth/register", reg_body.dump(), "application/json");
    ASSERT_TRUE(dup_res);
    EXPECT_EQ(dup_res->status, 409);

    // 8) Wrong password -> 401
    json bad_login;
    bad_login["account"] = "task6-route-reg";
    bad_login["password"] = "wrongRoutePass";
    auto bad_res = cli.Post("/api/v1/auth/login", bad_login.dump(), "application/json");
    ASSERT_TRUE(bad_res);
    EXPECT_EQ(bad_res->status, 401);

    // 9) Missing account -> 400
    json missing;
    missing["password"] = "pass";
    auto missing_res = cli.Post("/api/v1/auth/register", missing.dump(), "application/json");
    ASSERT_TRUE(missing_res);
    EXPECT_EQ(missing_res->status, 400);

    // Also remove the test user from DB
    odb::transaction t(db_->begin());
    db_->execute(R"(DELETE FROM "im_users" WHERE "account" = 'task6-route-reg')");
    t.commit();
}

} // anonymous namespace
