#include <gtest/gtest.h>

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

#include <message.hpp>
#include <message-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/http/message_client.hpp>
#include <gateway/http/message_http_controller.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <message_service.hpp>
#include <message_repository.hpp>
#include <push_notifier.hpp>
#include <utils/log_manager.hpp>

#include "../support/postgres_schema.hpp"

// Route registration free function (defined in gateway_server.cpp)
void register_message_http_routes_on_server(
    httplib::Server& server,
    im::gateway::MessageHttpController& controller);

namespace {

using json = nlohmann::json;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::MessageHttpController;
using im::gateway::LocalMessageClient;
using im::gateway::MultiPlatformAuthManager;
using im::service::message::MessageService;
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

struct PushCall {
    std::string receiver_uid;
    uint64_t msg_id = 0;
    std::string content;
    im::service::push::PushContext context;
};

class RecordingPushNotifier : public im::service::push::PushNotifier {
public:
    void notify_user(const std::string& receiver_uid,
                     uint64_t msg_id,
                     const std::string& content,
                     const im::service::push::PushContext& context) override {
        calls.push_back({receiver_uid, msg_id, content, context});
    }

    std::vector<PushCall> calls;
};

class GatewayMessageHttpTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("auth_mgr");
        LogManager::SetLogToConsole("redis_manager");
        LogManager::SetLogToConsole("message_http_controller");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        CleanupTestData();

        auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(
            "test_secret_key_for_gateway_message_http_tests", config_path());
        msg_service_ = std::make_shared<MessageService>(db_);
        msg_client_ = std::make_shared<LocalMessageClient>(msg_service_);
        controller_ = std::make_unique<MessageHttpController>(msg_client_, auth_mgr_);
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
        db_->execute(R"(DELETE FROM "im_messages"
            WHERE "sender_uid" LIKE 'task4-test-%'
               OR "receiver_uid" LIKE 'task4-test-%')");
        t.commit();
    }

    static std::string make_token_for(MultiPlatformAuthManager& mgr,
                                      const std::string& uid) {
        return mgr.generate_access_token(uid, uid + "-account", "test-device", "web", 3600);
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<MessageService> msg_service_;
    std::shared_ptr<LocalMessageClient> msg_client_;
    std::unique_ptr<MessageHttpController> controller_;
    RecordingPushNotifier recording_notifier_;
};

json parse_body(const httplib::Response& res) {
    return json::parse(res.body);
}

TEST_F(GatewayMessageHttpTest, SendMessageWithValidTokenReturns201) {
    std::string sender_uid = "task4-test-send-201-sender";
    std::string receiver_uid = "task4-test-send-201-receiver";
    std::string token = make_token_for(*auth_mgr_, sender_uid);

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    json body;
    body["receiver_uid"] = receiver_uid;
    body["content"] = "Hello from test!";
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send(req, res);

    EXPECT_EQ(res.status, 201) << "Body: " << res.body;
    json j = parse_body(res);
    ASSERT_TRUE(j.contains("message"));
    EXPECT_EQ(j["message"]["sender_uid"], sender_uid);
    EXPECT_EQ(j["message"]["receiver_uid"], receiver_uid);
    EXPECT_EQ(j["message"]["content"], "Hello from test!");
    EXPECT_GT(j["message"]["msg_id"].get<uint64_t>(), 0);
}

TEST_F(GatewayMessageHttpTest, SendMessageNotifiesReceiverThroughBoundary) {
    std::string sender_uid = "task4-test-http-push-sender";
    std::string receiver_uid = "task4-test-http-push-receiver";
    std::string token = make_token_for(*auth_mgr_, sender_uid);
    controller_->set_push_notifier(&recording_notifier_);

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    req.body = json{
        {"receiver_uid", receiver_uid},
        {"content", "HTTP push boundary"}
    }.dump();

    httplib::Response res;
    controller_->handle_send(req, res);

    EXPECT_EQ(res.status, 201) << "Body: " << res.body;
    json j = parse_body(res);
    const auto msg_id = j["message"]["msg_id"].get<uint64_t>();

    ASSERT_EQ(recording_notifier_.calls.size(), 1u);
    EXPECT_EQ(recording_notifier_.calls[0].receiver_uid, receiver_uid);
    EXPECT_EQ(recording_notifier_.calls[0].msg_id, msg_id);
    EXPECT_EQ(recording_notifier_.calls[0].content, "HTTP push boundary");
    EXPECT_EQ(recording_notifier_.calls[0].context.sender_uid, sender_uid);
    EXPECT_EQ(recording_notifier_.calls[0].context.conversation_type, "direct");
    EXPECT_EQ(recording_notifier_.calls[0].context.conversation_id, sender_uid);
}

TEST_F(GatewayMessageHttpTest, SendMessageMissingTokenReturns401) {
    httplib::Request req;
    req.method = "POST";
    json body;
    body["receiver_uid"] = "task4-test-no-token-rec";
    body["content"] = "No token";
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send(req, res);

    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayMessageHttpTest, SendMessageInvalidTokenReturns401) {
    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer definitely-invalid-token");
    json body;
    body["receiver_uid"] = "task4-test-bad-token-rec";
    body["content"] = "Bad token";
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send(req, res);

    EXPECT_EQ(res.status, 401) << "Body: " << res.body;
}

TEST_F(GatewayMessageHttpTest, SendMessageMissingFieldsReturns400) {
    std::string token = make_token_for(*auth_mgr_, "task4-test-missing-fields");

    // Missing receiver_uid
    {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + token);
        json body;
        body["content"] = "missing receiver";
        req.body = body.dump();

        httplib::Response res;
        controller_->handle_send(req, res);
        EXPECT_EQ(res.status, 400) << "Body: " << res.body;
    }

    // Missing content
    {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + token);
        json body;
        body["receiver_uid"] = "task4-test-missing-content";
        req.body = body.dump();

        httplib::Response res;
        controller_->handle_send(req, res);
        EXPECT_EQ(res.status, 400) << "Body: " << res.body;
    }
}

TEST_F(GatewayMessageHttpTest, SendMessageTrustsTokenUidNotBodyUid) {
    // The sender_uid in the request body should be ignored; the token
    // user is always the sender.
    std::string token_uid = "task4-test-trust-token";
    std::string receiver_uid = "task4-test-trust-rec";
    std::string token = make_token_for(*auth_mgr_, token_uid);

    httplib::Request req;
    req.method = "POST";
    req.set_header("Authorization", "Bearer " + token);
    json body;
    body["sender_uid"] = "task4-test-impersonator";
    body["receiver_uid"] = receiver_uid;
    body["content"] = "Trust token";
    req.body = body.dump();

    httplib::Response res;
    controller_->handle_send(req, res);

    EXPECT_EQ(res.status, 201) << "Body: " << res.body;
    json j = parse_body(res);
    EXPECT_EQ(j["message"]["sender_uid"], token_uid);
    EXPECT_NE(j["message"]["sender_uid"].get<std::string>(), "task4-test-impersonator");
}

TEST_F(GatewayMessageHttpTest, HistoryQueryReturnsMessages) {
    // Send two messages via the controller
    std::string sender_uid = "task4-test-hist-sender";
    std::string receiver_uid = "task4-test-hist-rec";
    std::string token = make_token_for(*auth_mgr_, sender_uid);

    auto send_msg = [&](const std::string& content) {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + token);
        json body;
        body["receiver_uid"] = receiver_uid;
        body["content"] = content;
        req.body = body.dump();
        httplib::Response res;
        controller_->handle_send(req, res);
        EXPECT_EQ(res.status, 201);
    };

    send_msg("history message 1");
    send_msg("history message 2");

    // Query history
    httplib::Request hist_req;
    hist_req.method = "GET";
    hist_req.set_header("Authorization", "Bearer " + token);
    hist_req.params.emplace("peer_uid", receiver_uid);

    httplib::Response hist_res;
    controller_->handle_history(hist_req, hist_res);

    EXPECT_EQ(hist_res.status, 200) << "Body: " << hist_res.body;
    json j = parse_body(hist_res);
    ASSERT_TRUE(j.contains("messages"));
    ASSERT_GE(j["messages"].size(), 2);
    EXPECT_EQ(j["messages"][0]["content"], "history message 1");
    EXPECT_EQ(j["messages"][1]["content"], "history message 2");
}

TEST_F(GatewayMessageHttpTest, HistoryQueryMissingPeerReturns400) {
    std::string token = make_token_for(*auth_mgr_, "task4-test-hist-no-peer");

    httplib::Request req;
    req.method = "GET";
    req.set_header("Authorization", "Bearer " + token);

    httplib::Response res;
    controller_->handle_history(req, res);

    EXPECT_EQ(res.status, 400) << "Body: " << res.body;
}

TEST_F(GatewayMessageHttpTest, OfflinePullReturnsAndDeliversMessages) {
    // Send a message from A to B
    std::string sender_uid = "task4-test-off-sender";
    std::string receiver_uid = "task4-test-off-rec";
    std::string sender_token = make_token_for(*auth_mgr_, sender_uid);

    {
        httplib::Request req;
        req.method = "POST";
        req.set_header("Authorization", "Bearer " + sender_token);
        json body;
        body["receiver_uid"] = receiver_uid;
        body["content"] = "Offline message";
        req.body = body.dump();
        httplib::Response res;
        controller_->handle_send(req, res);
        ASSERT_EQ(res.status, 201);
    }

    // Pull offline as receiver
    std::string recv_token = make_token_for(*auth_mgr_, receiver_uid);

    httplib::Request off_req;
    off_req.method = "GET";
    off_req.set_header("Authorization", "Bearer " + recv_token);

    httplib::Response off_res;
    controller_->handle_offline(off_req, off_res);

    EXPECT_EQ(off_res.status, 200) << "Body: " << off_res.body;
    json j = parse_body(off_res);
    ASSERT_TRUE(j.contains("messages"));
    ASSERT_GE(j["messages"].size(), 1);
    EXPECT_EQ(j["messages"][0]["sender_uid"], sender_uid);
    EXPECT_EQ(j["messages"][0]["content"], "Offline message");
    EXPECT_EQ(j["messages"][0]["status"], static_cast<int>(im::service::message::MessageStatus::DELIVERED));

    // Second pull should be empty (messages were marked delivered)
    {
        httplib::Response off_res2;
        controller_->handle_offline(off_req, off_res2);
        EXPECT_EQ(off_res2.status, 200);
        json j2 = parse_body(off_res2);
        ASSERT_TRUE(j2.contains("messages"));
        EXPECT_EQ(j2["messages"].size(), 0) << "Second offline pull should be empty: " << off_res2.body;
    }
}

TEST_F(GatewayMessageHttpTest, RoutesAreRegisteredAndHandleRequests) {
    httplib::Server svr;
    register_message_http_routes_on_server(svr, *controller_);
    svr.Get(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 599;
        res.set_content("catch-all", "text/plain");
    });
    svr.Post(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 599;
        res.set_content("catch-all", "text/plain");
    });

    int test_port = 18568;
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

    std::string sender_uid = "task4-route-sender";
    std::string receiver_uid = "task4-route-rec";
    std::string token = make_token_for(*auth_mgr_, sender_uid);

    // 1) Send message
    json send_body;
    send_body["receiver_uid"] = receiver_uid;
    send_body["content"] = "Route test message";
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto send_res = cli.Post("/api/v1/messages/send", headers, send_body.dump(), "application/json");
    ASSERT_TRUE(send_res) << "HTTP request failed";
    EXPECT_EQ(send_res->status, 201);
    json send_j = json::parse(send_res->body);
    ASSERT_TRUE(send_j.contains("message"));
    EXPECT_EQ(send_j["message"]["sender_uid"], sender_uid);

    // 2) History query
    auto hist_res = cli.Get("/api/v1/messages/history?peer_uid=" + receiver_uid, headers);
    ASSERT_TRUE(hist_res);
    EXPECT_EQ(hist_res->status, 200);
    json hist_j = json::parse(hist_res->body);
    EXPECT_TRUE(hist_j.contains("messages"));

    // 3) Send without auth → 401
    auto unauth_res = cli.Post("/api/v1/messages/send", send_body.dump(), "application/json");
    ASSERT_TRUE(unauth_res);
    EXPECT_EQ(unauth_res->status, 401);

    // 4) History without auth → 401
    auto unauth_hist = cli.Get("/api/v1/messages/history?peer_uid=" + receiver_uid);
    ASSERT_TRUE(unauth_hist);
    EXPECT_EQ(unauth_hist->status, 401);
}

} // anonymous namespace
