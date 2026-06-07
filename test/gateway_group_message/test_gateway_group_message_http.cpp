#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <group_message.hpp>
#include <group_message-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/http/group_message_http_controller.hpp>
#include <gateway/http/group_client.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <push_notifier.hpp>
#include <group_message_service.hpp>
#include <group_service.hpp>
#include <user_service.hpp>
#include <password_hasher.hpp>
#include <utils/log_manager.hpp>

#include "../support/postgres_schema.hpp"

// Route registration free function (defined in gateway_server.cpp)
void register_group_message_http_routes_on_server(
    httplib::Server& server,
    im::gateway::GroupMessageHttpController& controller);

namespace {

using json = nlohmann::json;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::GroupMessageHttpController;
using im::gateway::LocalGroupClient;
using im::gateway::MultiPlatformAuthManager;
using im::service::group::GroupService;
using im::service::group::GroupMessageService;
using im::service::group::CreateGroupRequest;
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

struct PushCall {
    std::string receiver_uid;
    uint64_t msg_id;
    std::string content;
};

class RecordingPushNotifier : public im::service::push::PushNotifier {
public:
    void notify_user(const std::string& receiver_uid,
                     uint64_t msg_id,
                     const std::string& content) override {
        calls.push_back({receiver_uid, msg_id, content});
    }

    std::vector<PushCall> calls;
};

class GatewayGroupMessageHttpTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("auth_mgr");
        LogManager::SetLogToConsole("redis_manager");
        LogManager::SetLogToConsole("group_message_http_controller");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTables();
        CleanupTestData();

        auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(
            "test_secret_key_for_gateway_group_message_http_tests", config_path());
        auto hasher = std::make_unique<PasswordHasher>();
        user_svc_ = std::make_shared<UserService>(db_, std::move(hasher));
        group_svc_ = std::make_shared<GroupService>(db_, user_svc_);
        group_msg_svc_ = std::make_shared<GroupMessageService>(db_, user_svc_, group_svc_);
        group_client_ = std::make_shared<LocalGroupClient>(group_svc_, group_msg_svc_);
        // PushService is null in tests — fanout is tested separately
        controller_ = std::make_unique<GroupMessageHttpController>(
            group_client_, auth_mgr_, nullptr);
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
        db_->execute(R"(DELETE FROM "im_group_messages"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'group-msg-http-test-%'
                )
               OR "sender_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'group-msg-http-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_group_members"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'group-msg-http-test-%'
                )
               OR "user_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'group-msg-http-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_groups"
            WHERE "creator_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'group-msg-http-test-%'
                )
               OR "name" LIKE 'group-msg-http-test-%')");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'group-msg-http-test-%')");
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

    uint64_t create_group(const std::string& name, const std::string& creator_uid) {
        CreateGroupRequest req;
        req.name = name;
        req.creator_uid = creator_uid;
        req.now_ms = kNowMs;
        auto result = group_svc_->create_group(req);
        EXPECT_TRUE(result.ok);
        return result.group_id;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<UserService> user_svc_;
    std::shared_ptr<GroupService> group_svc_;
    std::shared_ptr<GroupMessageService> group_msg_svc_;
    std::shared_ptr<im::gateway::GroupClient> group_client_;
    std::unique_ptr<GroupMessageHttpController> controller_;
    RecordingPushNotifier recording_notifier_;
};

json parse_body(const httplib::Response& res) {
    return json::parse(res.body);
}

// ==================== Send Message Tests ====================

TEST_F(GatewayGroupMessageHttpTest, SendMessageRequiresAuth) {
    httplib::Request req;
    req.method = "POST";
    req.body = R"({"group_id":1,"content":"hello"})";

    httplib::Response res;
    controller_->handle_send_message(req, res);

    EXPECT_EQ(res.status, 401);
}

TEST_F(GatewayGroupMessageHttpTest, SendMessageHappyPath) {
    std::string owner = create_user("group-msg-http-test-send-owner");
    std::string member = create_user("group-msg-http-test-send-member");
    std::string token = make_token(owner);

    uint64_t gid = create_group("group-msg-http-test-send-group", owner);
    ASSERT_TRUE(group_svc_->join_group(gid, member, kNowMs).ok);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", gid}, {"content", "Hello group!"}}.dump();

    httplib::Response res;
    controller_->handle_send_message(req, res);

    EXPECT_EQ(res.status, 201);
    auto body = parse_body(res);
    EXPECT_EQ(body["message"], "Message sent");
    EXPECT_GT(body["msg_id"].get<uint64_t>(), 0);
}

TEST_F(GatewayGroupMessageHttpTest, SendMessageFansOutToOtherMembersOnly) {
    std::string owner = create_user("group-msg-http-test-fanout-owner");
    std::string member1 = create_user("group-msg-http-test-fanout-member1");
    std::string member2 = create_user("group-msg-http-test-fanout-member2");
    std::string token = make_token(owner);

    uint64_t gid = create_group("group-msg-http-test-fanout-group", owner);
    ASSERT_TRUE(group_svc_->join_group(gid, member1, kNowMs).ok);
    ASSERT_TRUE(group_svc_->join_group(gid, member2, kNowMs).ok);
    controller_->set_push_notifier(&recording_notifier_);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", gid}, {"content", "Fanout group!"}}.dump();

    httplib::Response res;
    controller_->handle_send_message(req, res);

    EXPECT_EQ(res.status, 201);
    auto body = parse_body(res);
    uint64_t msg_id = body["msg_id"].get<uint64_t>();

    ASSERT_EQ(recording_notifier_.calls.size(), 2u);
    std::vector<std::string> receivers = {
        recording_notifier_.calls[0].receiver_uid,
        recording_notifier_.calls[1].receiver_uid,
    };
    EXPECT_NE(std::find(receivers.begin(), receivers.end(), member1), receivers.end());
    EXPECT_NE(std::find(receivers.begin(), receivers.end(), member2), receivers.end());
    EXPECT_EQ(std::find(receivers.begin(), receivers.end(), owner), receivers.end());

    for (const auto& call : recording_notifier_.calls) {
        EXPECT_EQ(call.msg_id, msg_id);
        EXPECT_EQ(call.content, "Fanout group!");
    }
}

TEST_F(GatewayGroupMessageHttpTest, SendMessageMissingGroupId) {
    std::string uid = create_user("group-msg-http-test-send-no-gid");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = R"({"content":"hello"})";

    httplib::Response res;
    controller_->handle_send_message(req, res);

    EXPECT_EQ(res.status, 400);
}

TEST_F(GatewayGroupMessageHttpTest, SendMessageMissingContent) {
    std::string uid = create_user("group-msg-http-test-send-no-content");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = R"({"group_id":1})";

    httplib::Response res;
    controller_->handle_send_message(req, res);

    EXPECT_EQ(res.status, 400);
}

TEST_F(GatewayGroupMessageHttpTest, SendMessageInvalidJson) {
    std::string uid = create_user("group-msg-http-test-send-bad-json");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = "not-json";

    httplib::Response res;
    controller_->handle_send_message(req, res);

    EXPECT_EQ(res.status, 400);
}

// ==================== Get History Tests ====================

TEST_F(GatewayGroupMessageHttpTest, GetHistoryRequiresAuth) {
    httplib::Request req;
    req.method = "GET";

    httplib::Response res;
    controller_->handle_get_history(req, res);

    EXPECT_EQ(res.status, 401);
}

TEST_F(GatewayGroupMessageHttpTest, GetHistoryHappyPath) {
    std::string owner = create_user("group-msg-http-test-hist-owner");
    std::string member = create_user("group-msg-http-test-hist-member");
    std::string token = make_token(member);

    uint64_t gid = create_group("group-msg-http-test-hist-group", owner);
    ASSERT_TRUE(group_svc_->join_group(gid, member, kNowMs).ok);

    // Send a message first
    ASSERT_TRUE(group_msg_svc_->send_message(gid, owner, "First message", kNowMs).ok);
    ASSERT_TRUE(group_msg_svc_->send_message(gid, member, "Second message", kNowMs + 1000).ok);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", std::to_string(gid));

    httplib::Response res;
    controller_->handle_get_history(req, res);

    EXPECT_EQ(res.status, 200);
    auto body = parse_body(res);
    ASSERT_TRUE(body.contains("messages"));
    ASSERT_EQ(body["messages"].size(), 2);
    EXPECT_EQ(body["messages"][0]["content"], "Second message");
    EXPECT_EQ(body["messages"][1]["content"], "First message");
}

TEST_F(GatewayGroupMessageHttpTest, GetHistoryNonMemberForbidden) {
    std::string owner = create_user("group-msg-http-test-hist-nm-owner");
    std::string stranger = create_user("group-msg-http-test-hist-nm-stranger");
    std::string token = make_token(stranger);

    uint64_t gid = create_group("group-msg-http-test-hist-nm-group", owner);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", std::to_string(gid));

    httplib::Response res;
    controller_->handle_get_history(req, res);

    EXPECT_EQ(res.status, 403);
}

TEST_F(GatewayGroupMessageHttpTest, GetHistoryNonexistentGroup) {
    std::string uid = create_user("group-msg-http-test-hist-nf");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", "99999999");

    httplib::Response res;
    controller_->handle_get_history(req, res);

    EXPECT_EQ(res.status, 404);
}

TEST_F(GatewayGroupMessageHttpTest, GetHistoryMissingGroupId) {
    std::string uid = create_user("group-msg-http-test-hist-no-gid");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);

    httplib::Response res;
    controller_->handle_get_history(req, res);

    EXPECT_EQ(res.status, 400);
}

TEST_F(GatewayGroupMessageHttpTest, GetHistoryWithBeforeTime) {
    std::string owner = create_user("group-msg-http-test-hist-before-owner");
    std::string member = create_user("group-msg-http-test-hist-before-member");
    std::string token = make_token(member);

    uint64_t gid = create_group("group-msg-http-test-hist-before-group", owner);
    ASSERT_TRUE(group_svc_->join_group(gid, member, kNowMs).ok);

    ASSERT_TRUE(group_msg_svc_->send_message(gid, owner, "Old message", kNowMs).ok);
    ASSERT_TRUE(group_msg_svc_->send_message(gid, member, "New message", kNowMs + 10000).ok);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", std::to_string(gid));
    req.params.emplace("before", std::to_string(kNowMs + 5000));

    httplib::Response res;
    controller_->handle_get_history(req, res);

    EXPECT_EQ(res.status, 200);
    auto body = parse_body(res);
    ASSERT_TRUE(body.contains("messages"));
    ASSERT_EQ(body["messages"].size(), 1);
    EXPECT_EQ(body["messages"][0]["content"], "Old message");
}

// ==================== Send Message Permission Tests ====================

TEST_F(GatewayGroupMessageHttpTest, SendMessageNonMemberForbidden) {
    std::string owner = create_user("group-msg-http-test-perm-owner");
    std::string stranger = create_user("group-msg-http-test-perm-stranger");
    std::string token = make_token(stranger);

    uint64_t gid = create_group("group-msg-http-test-perm-group", owner);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", gid}, {"content", "Hello"}}.dump();

    httplib::Response res;
    controller_->handle_send_message(req, res);

    EXPECT_EQ(res.status, 403);
}

TEST_F(GatewayGroupMessageHttpTest, SendMessageToNonexistentGroup) {
    std::string uid = create_user("group-msg-http-test-send-nonexist");
    std::string token = make_token(uid);

    httplib::Request req;
    req.method = "POST";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.body = json{{"group_id", 99999999ULL}, {"content", "Hello"}}.dump();

    httplib::Response res;
    controller_->handle_send_message(req, res);

    EXPECT_EQ(res.status, 404);
}

// ==================== History Parameter Validation ====================

TEST_F(GatewayGroupMessageHttpTest, GetHistoryInvalidBefore) {
    std::string owner = create_user("group-msg-http-test-hist-bad-before");
    std::string token = make_token(owner);

    uint64_t gid = create_group("group-msg-http-test-hist-bad-before-group", owner);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", std::to_string(gid));
    req.params.emplace("before", "not-a-number");

    httplib::Response res;
    controller_->handle_get_history(req, res);

    EXPECT_EQ(res.status, 400);
}

TEST_F(GatewayGroupMessageHttpTest, GetHistoryInvalidLimit) {
    std::string owner = create_user("group-msg-http-test-hist-bad-limit");
    std::string token = make_token(owner);

    uint64_t gid = create_group("group-msg-http-test-hist-bad-limit-group", owner);

    httplib::Request req;
    req.method = "GET";
    req.headers.emplace("Authorization", "Bearer " + token);
    req.params.emplace("group_id", std::to_string(gid));
    req.params.emplace("limit", "not-a-number");

    httplib::Response res;
    controller_->handle_get_history(req, res);

    EXPECT_EQ(res.status, 400);
}

} // anonymous namespace
