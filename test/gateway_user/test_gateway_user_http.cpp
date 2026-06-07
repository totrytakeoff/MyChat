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
json parse_body(const httplib::Response& res) {
    return json::parse(res.body);
}

// Helper: register a user through the controller, return the response
httplib::Response do_register(UserHttpController& ctrl,
                              const std::string& account,
                              const std::string& password,
                              const std::string& nickname = "") {
    httplib::Request req;
    json body;
    body["account"] = account;
    body["password"] = password;
    if (!nickname.empty()) body["nickname"] = nickname;
    req.body = body.dump();
    req.method = "POST";

    httplib::Response res;
    ctrl.handle_register(req, res);
    return res;
}

// Helper: login through the controller
httplib::Response do_login(UserHttpController& ctrl,
                           const std::string& account,
                           const std::string& password) {
    httplib::Request req;
    json body;
    body["account"] = account;
    body["password"] = password;
    req.body = body.dump();
    req.method = "POST";

    httplib::Response res;
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
    httplib::Request req;
    json body;
    body["account"] = "task6-test-missing";
    req.body = body.dump();
    req.method = "POST";

    httplib::Response res;
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

    httplib::Request req;
    req.method = "GET";
    req.set_header("Authorization", "Bearer " + access_token);

    httplib::Response res;
    controller_->handle_profile(req, res);
    EXPECT_EQ(res.status, 200) << "Body: " << res.body;

    json j = parse_body(res);
    ASSERT_TRUE(j.contains("profile"));
    EXPECT_EQ(j["profile"]["account"], "task6-test-profile-200");
}

TEST_F(GatewayUserHttpTest, ProfileMissingTokenReturns401) {
    httplib::Request req;
    req.method = "GET";

    httplib::Response res;
    controller_->handle_profile(req, res);
    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayUserHttpTest, ProfileInvalidTokenReturns401) {
    httplib::Request req;
    req.method = "GET";
    req.set_header("Authorization", "Bearer definitely-invalid-token");

    httplib::Response res;
    controller_->handle_profile(req, res);
    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayUserHttpTest, ProfileNonExistentUidReturns404) {
    httplib::Request req;
    req.method = "GET";
    auto token = auth_mgr_->generate_access_token(
        "task6-test-missing-uid", "missing-account", "device-web", "web", 60);
    req.set_header("Authorization", "Bearer " + token);

    httplib::Response res;
    controller_->handle_profile(req, res);
    EXPECT_EQ(res.status, 404) << "Body: " << res.body;
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

    // 4) Profile without token → 401
    auto unauth_res = cli.Get("/api/v1/auth/info");
    ASSERT_TRUE(unauth_res);
    EXPECT_EQ(unauth_res->status, 401);

    // 5) Duplicate register → 409
    auto dup_res = cli.Post("/api/v1/auth/register", reg_body.dump(), "application/json");
    ASSERT_TRUE(dup_res);
    EXPECT_EQ(dup_res->status, 409);

    // 6) Wrong password → 401
    json bad_login;
    bad_login["account"] = "task6-route-reg";
    bad_login["password"] = "wrongRoutePass";
    auto bad_res = cli.Post("/api/v1/auth/login", bad_login.dump(), "application/json");
    ASSERT_TRUE(bad_res);
    EXPECT_EQ(bad_res->status, 401);

    // 7) Missing account → 400
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
