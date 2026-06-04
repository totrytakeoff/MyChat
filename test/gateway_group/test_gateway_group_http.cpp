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

#include <group.hpp>
#include <group-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/http/group_http_controller.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <group_service.hpp>
#include <user_service.hpp>
#include <password_hasher.hpp>
#include <utils/log_manager.hpp>

// Route registration free function (defined in gateway_server.cpp)
void register_group_http_routes_on_server(
    httplib::Server& server,
    im::gateway::GroupHttpController& controller);

namespace {

using json = nlohmann::json;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::GroupHttpController;
using im::gateway::MultiPlatformAuthManager;
using im::service::group::GroupService;
using im::service::group::CreateGroupRequest;
using im::service::group::GroupInfoDTO;
using im::service::group::GroupRole;
using im::service::user::UserService;
using im::service::user::PasswordHasher;
using im::service::user::RegisterRequest;
using im::utils::LogManager;

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1720000000000;

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

class GatewayGroupHttpTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("auth_mgr");
        LogManager::SetLogToConsole("redis_manager");
        LogManager::SetLogToConsole("group_http_controller");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTables();
        CleanupTestData();

        auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(
            "test_secret_key_for_gateway_group_http_tests", config_path());
        auto hasher = std::make_unique<PasswordHasher>();
        auto user_svc = std::make_shared<UserService>(db_, std::move(hasher));
        group_svc_ = std::make_shared<GroupService>(db_, user_svc);
        controller_ = std::make_unique<GroupHttpController>(group_svc_, auth_mgr_);
    }

    void TearDown() override {
        CleanupTestData();
        controller_.reset();
        auth_mgr_.reset();
        redis_manager().shutdown();
    }

    static void EnsureTables() {
        odb::pgsql::database db(kConnStr);
        odb::transaction t(db.begin());
        db.execute(R"(
            CREATE TABLE IF NOT EXISTS "im_users" (
                "uid" TEXT NOT NULL PRIMARY KEY,
                "account" TEXT NOT NULL,
                "password_hash" TEXT NOT NULL,
                "nickname" TEXT NOT NULL,
                "avatar" TEXT NOT NULL,
                "gender" INTEGER NOT NULL,
                "signature" TEXT NOT NULL,
                "create_time" BIGINT NOT NULL,
                "last_login" BIGINT NOT NULL,
                "online" BOOLEAN NOT NULL
            )
        )");
        db.execute(R"(
            CREATE UNIQUE INDEX IF NOT EXISTS "im_users_account_i"
                ON "im_users" ("account")
        )");
        db.execute(R"(
            CREATE TABLE IF NOT EXISTS "im_groups" (
                "group_id" BIGSERIAL NOT NULL PRIMARY KEY,
                "name" TEXT NOT NULL,
                "creator_uid" TEXT NOT NULL,
                "created_at" BIGINT NOT NULL
            )
        )");
        db.execute(R"(
            CREATE TABLE IF NOT EXISTS "im_group_members" (
                "id" BIGSERIAL NOT NULL PRIMARY KEY,
                "group_id" BIGINT NOT NULL,
                "user_uid" TEXT NOT NULL,
                "role" INTEGER NOT NULL,
                "joined_at" BIGINT NOT NULL
            )
        )");
        db.execute(R"(
            CREATE UNIQUE INDEX IF NOT EXISTS "im_group_members_pair_i"
                ON "im_group_members" ("group_id", "user_uid")
        )");
        t.commit();
    }

    void CleanupTestData() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_group_members"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'group-http-test-%'
                )
               OR "user_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'group-http-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_groups"
            WHERE "creator_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'group-http-test-%'
                )
               OR "name" LIKE 'group-http-test-%')");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'group-http-test-%')");
        t.commit();
    }

    std::string make_token(const std::string& uid) {
        return auth_mgr_->generate_access_token(uid, uid + "-acct", "test-device", "web", 3600);
    }

    std::string create_user(const std::string& account) {
        RegisterRequest req;
        req.account = account;
        req.password = "testPass123";
        req.nickname = account;
        req.now_ms = kNowMs;
        auto hasher = std::make_unique<PasswordHasher>();
        auto svc = UserService(db_, std::move(hasher));
        auto result = svc.register_user(req);
        EXPECT_TRUE(result.ok) << "Failed to create user " << account;
        return result.profile.uid;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<GroupService> group_svc_;
    std::unique_ptr<GroupHttpController> controller_;
};

json parse_body(const httplib::Response& res) {
    return json::parse(res.body);
}

// ==================== Create Group Tests ====================

TEST_F(GatewayGroupHttpTest, CreateGroupRequiresAuth) {
    httplib::Request req;
    req.method = "POST";
    req.body = R"({"name":"test-group"})";

    httplib::Response res;
    controller_->handle_create_group(req, res);

    EXPECT_EQ(res.status, 401);
}

TEST_F(GatewayGroupHttpTest, CreateGroupHappyPath) {
    std::string uid = create_user("group-http-test-create-ok");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = R"({"name":"group-http-test-create-ok-group"})";

    httplib::Response res;
    controller_->handle_create_group(req, res);

    EXPECT_EQ(res.status, 201);
    auto body = parse_body(res);
    EXPECT_EQ(body["message"], "Group created");
    EXPECT_GT(body["group_id"].get<uint64_t>(), 0);
}

TEST_F(GatewayGroupHttpTest, CreateGroupMissingName) {
    std::string uid = create_user("group-http-test-create-no-name");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = R"({})";

    httplib::Response res;
    controller_->handle_create_group(req, res);

    EXPECT_EQ(res.status, 400);
}

TEST_F(GatewayGroupHttpTest, CreateGroupInvalidJson) {
    std::string uid = create_user("group-http-test-create-bad-json");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = "not-json";

    httplib::Response res;
    controller_->handle_create_group(req, res);

    EXPECT_EQ(res.status, 400);
}

TEST_F(GatewayGroupHttpTest, CreateGroupWithInvalidToken) {
    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer invalid-token");
    req.body = R"({"name":"test-group"})";

    httplib::Response res;
    controller_->handle_create_group(req, res);

    EXPECT_EQ(res.status, 401);
}

// ==================== Join Group Tests ====================

TEST_F(GatewayGroupHttpTest, JoinGroupRequiresAuth) {
    httplib::Request req;
    req.method = "POST";
    req.body = R"({"group_id":1})";

    httplib::Response res;
    controller_->handle_join_group(req, res);

    EXPECT_EQ(res.status, 401);
}

TEST_F(GatewayGroupHttpTest, JoinGroupHappyPath) {
    std::string uid_owner = create_user("group-http-test-join-owner");
    std::string uid_member = create_user("group-http-test-join-member");
    std::string token = make_token(uid_member);

    // Create group first
    CreateGroupRequest creq;
    creq.name = "group-http-test-join-group";
    creq.creator_uid = uid_owner;
    creq.now_ms = kNowMs;
    auto cres = group_svc_->create_group(creq);
    ASSERT_TRUE(cres.ok);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", cres.group_id}}.dump();

    httplib::Response res;
    controller_->handle_join_group(req, res);

    EXPECT_EQ(res.status, 200);
    auto body = parse_body(res);
    EXPECT_EQ(body["message"], "Joined group");
}

TEST_F(GatewayGroupHttpTest, JoinGroupNotFound) {
    std::string uid = create_user("group-http-test-join-nf");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = R"({"group_id":99999999})";

    httplib::Response res;
    controller_->handle_join_group(req, res);

    EXPECT_EQ(res.status, 404);
}

TEST_F(GatewayGroupHttpTest, JoinGroupAlreadyMember) {
    std::string uid_owner = create_user("group-http-test-join-dup-owner");
    std::string uid_member = create_user("group-http-test-join-dup-member");
    std::string token = make_token(uid_member);

    CreateGroupRequest creq;
    creq.name = "group-http-test-join-dup-group";
    creq.creator_uid = uid_owner;
    creq.now_ms = kNowMs;
    auto cres = group_svc_->create_group(creq);
    ASSERT_TRUE(cres.ok);

    // First join succeeds
    ASSERT_TRUE(group_svc_->join_group(cres.group_id, uid_member, kNowMs).ok);

    // Second join should fail
    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", cres.group_id}}.dump();

    httplib::Response res;
    controller_->handle_join_group(req, res);

    EXPECT_EQ(res.status, 409);
}

TEST_F(GatewayGroupHttpTest, JoinGroupMissingGroupId) {
    std::string uid = create_user("group-http-test-join-no-id");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = R"({})";

    httplib::Response res;
    controller_->handle_join_group(req, res);

    EXPECT_EQ(res.status, 400);
}

// ==================== Leave Group Tests ====================

TEST_F(GatewayGroupHttpTest, LeaveGroupRequiresAuth) {
    httplib::Request req;
    req.method = "POST";
    req.body = R"({"group_id":1})";

    httplib::Response res;
    controller_->handle_leave_group(req, res);

    EXPECT_EQ(res.status, 401);
}

TEST_F(GatewayGroupHttpTest, LeaveGroupHappyPath) {
    std::string uid_owner = create_user("group-http-test-leave-owner");
    std::string uid_member = create_user("group-http-test-leave-member");
    std::string token = make_token(uid_member);

    CreateGroupRequest creq;
    creq.name = "group-http-test-leave-group";
    creq.creator_uid = uid_owner;
    creq.now_ms = kNowMs;
    auto cres = group_svc_->create_group(creq);
    ASSERT_TRUE(cres.ok);

    ASSERT_TRUE(group_svc_->join_group(cres.group_id, uid_member, kNowMs).ok);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", cres.group_id}}.dump();

    httplib::Response res;
    controller_->handle_leave_group(req, res);

    EXPECT_EQ(res.status, 200);
    auto body = parse_body(res);
    EXPECT_EQ(body["message"], "Left group");
}

TEST_F(GatewayGroupHttpTest, LeaveGroupOwnerCannotLeave) {
    std::string uid = create_user("group-http-test-leave-owner-fail");
    std::string token = make_token(uid);

    CreateGroupRequest creq;
    creq.name = "group-http-test-leave-owner-fail-group";
    creq.creator_uid = uid;
    creq.now_ms = kNowMs;
    auto cres = group_svc_->create_group(creq);
    ASSERT_TRUE(cres.ok);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", cres.group_id}}.dump();

    httplib::Response res;
    controller_->handle_leave_group(req, res);

    EXPECT_EQ(res.status, 403);
}

TEST_F(GatewayGroupHttpTest, LeaveGroupNotMember) {
    std::string uid_owner = create_user("group-http-test-leave-nm-owner");
    std::string uid_stranger = create_user("group-http-test-leave-nm-stranger");
    std::string token = make_token(uid_stranger);

    CreateGroupRequest creq;
    creq.name = "group-http-test-leave-nm-group";
    creq.creator_uid = uid_owner;
    creq.now_ms = kNowMs;
    auto cres = group_svc_->create_group(creq);
    ASSERT_TRUE(cres.ok);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", cres.group_id}}.dump();

    httplib::Response res;
    controller_->handle_leave_group(req, res);

    EXPECT_EQ(res.status, 403);
}

TEST_F(GatewayGroupHttpTest, LeaveGroupNotFound) {
    std::string uid = create_user("group-http-test-leave-nf");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = R"({"group_id":99999999})";

    httplib::Response res;
    controller_->handle_leave_group(req, res);

    EXPECT_EQ(res.status, 404);
}

// ==================== List Groups Tests ====================

TEST_F(GatewayGroupHttpTest, ListGroupsRequiresAuth) {
    httplib::Request req;
    req.method = "GET";

    httplib::Response res;
    controller_->handle_list_groups(req, res);

    EXPECT_EQ(res.status, 401);
}

TEST_F(GatewayGroupHttpTest, ListGroupsReturnsMyGroups) {
    std::string uid_a = create_user("group-http-test-list-a");
    std::string token_a = make_token(uid_a);

    // Create and join groups
    CreateGroupRequest creq;
    creq.name = "group-http-test-list-g1";
    creq.creator_uid = uid_a;
    creq.now_ms = kNowMs;
    auto c1 = group_svc_->create_group(creq);
    ASSERT_TRUE(c1.ok);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token_a);

    httplib::Response res;
    controller_->handle_list_groups(req, res);

    EXPECT_EQ(res.status, 200);
    auto body = parse_body(res);
    EXPECT_TRUE(body.contains("groups"));
    ASSERT_GE(body["groups"].size(), 1);

    bool found = false;
    for (const auto& g : body["groups"]) {
        if (g["group_id"].get<uint64_t>() == c1.group_id) {
            found = true;
            EXPECT_EQ(g["name"], "group-http-test-list-g1");
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(GatewayGroupHttpTest, ListGroupsEmptyForNewUser) {
    std::string uid = create_user("group-http-test-list-empty");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);

    httplib::Response res;
    controller_->handle_list_groups(req, res);

    EXPECT_EQ(res.status, 200);
    auto body = parse_body(res);
    ASSERT_TRUE(body.contains("groups"));
    EXPECT_TRUE(body["groups"].empty());
}

// ==================== List Members Tests ====================

TEST_F(GatewayGroupHttpTest, ListMembersRequiresAuth) {
    httplib::Request req;
    req.method = "GET";
    req.params.emplace("group_id", "1");

    httplib::Response res;
    controller_->handle_list_members(req, res);

    EXPECT_EQ(res.status, 401);
}

TEST_F(GatewayGroupHttpTest, ListMembersHappyPath) {
    std::string uid_owner = create_user("group-http-test-members-owner");
    std::string uid_member = create_user("group-http-test-members-member");
    std::string token = make_token(uid_owner);

    CreateGroupRequest creq;
    creq.name = "group-http-test-members-group";
    creq.creator_uid = uid_owner;
    creq.now_ms = kNowMs;
    auto cres = group_svc_->create_group(creq);
    ASSERT_TRUE(cres.ok);

    ASSERT_TRUE(group_svc_->join_group(cres.group_id, uid_member, kNowMs).ok);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", std::to_string(cres.group_id));

    httplib::Response res;
    controller_->handle_list_members(req, res);

    EXPECT_EQ(res.status, 200);
    auto body = parse_body(res);
    ASSERT_TRUE(body.contains("members"));
    ASSERT_EQ(body["members"].size(), 2);
}

TEST_F(GatewayGroupHttpTest, ListMembersNonMemberGetsEmpty) {
    std::string uid_owner = create_user("group-http-test-members-nm-owner");
    std::string uid_stranger = create_user("group-http-test-members-nm-stranger");
    std::string token = make_token(uid_stranger);

    CreateGroupRequest creq;
    creq.name = "group-http-test-members-nm-group";
    creq.creator_uid = uid_owner;
    creq.now_ms = kNowMs;
    auto cres = group_svc_->create_group(creq);
    ASSERT_TRUE(cres.ok);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", std::to_string(cres.group_id));

    httplib::Response res;
    controller_->handle_list_members(req, res);

    EXPECT_EQ(res.status, 403);
}

TEST_F(GatewayGroupHttpTest, ListMembersNonexistentGroup) {
    std::string uid = create_user("group-http-test-members-nf");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", "99999999");

    httplib::Response res;
    controller_->handle_list_members(req, res);

    EXPECT_EQ(res.status, 404);
}

TEST_F(GatewayGroupHttpTest, ListMembersMissingGroupId) {
    std::string uid = create_user("group-http-test-members-no-id");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);

    httplib::Response res;
    controller_->handle_list_members(req, res);

    EXPECT_EQ(res.status, 400);
}

} // anonymous namespace
