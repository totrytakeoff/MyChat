#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <message.hpp>
#include <message-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/ws/message_ws_handler.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <gateway/connection_manager/connection_manager.hpp>
#include <gateway/push/push_service.hpp>
#include <push_notifier.hpp>
#include <message_service.hpp>
#include <message_repository.hpp>
#include <utils/log_manager.hpp>

#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/proto/command.pb.h"
#include "../../common/proto/message.pb.h"

namespace {

using json = nlohmann::json;
using im::base::ErrorCode;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::ConnectionManager;
using im::gateway::MessageWsHandler;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::ProcessorResult;
using im::gateway::PushService;
using im::gateway::UnifiedMessage;
using im::network::ProtobufCodec;
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

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
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

class GatewayMessageWsTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("message_ws_handler");
        LogManager::SetLogToConsole("auth_mgr");
        LogManager::SetLogToConsole("redis_manager");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        CleanupTestData();

        auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(
            "test_secret_key_for_gateway_message_ws_tests", config_path());
        msg_service_ = std::make_shared<MessageService>(db_);
        // Default: construct without push infrastructure (null conn_mgr, null ws_server)
        ws_handler_ = std::make_unique<MessageWsHandler>(msg_service_, auth_mgr_, nullptr);
    }

    void TearDown() override {
        CleanupTestData();
        ws_handler_.reset();
        conn_mgr_.reset();
        msg_service_.reset();
        auth_mgr_.reset();
        redis_manager().shutdown();
    }

    static void EnsureTable() {
        odb::pgsql::database db(kConnStr);
        odb::transaction t(db.begin());
        db.execute(R"(
            CREATE TABLE IF NOT EXISTS "im_messages" (
                "msg_id" BIGSERIAL NOT NULL PRIMARY KEY,
                "sender_uid" TEXT NOT NULL,
                "receiver_uid" TEXT NOT NULL,
                "content" TEXT NOT NULL,
                "msg_type" INTEGER NOT NULL,
                "status" INTEGER NOT NULL,
                "create_time" BIGINT NOT NULL,
                "delivered_time" BIGINT NOT NULL,
                "read_time" BIGINT NOT NULL
            )
        )");
        t.commit();
    }

    void CleanupTestData() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_messages"
            WHERE "sender_uid" LIKE 'task8-test-%'
               OR "receiver_uid" LIKE 'task8-test-%')");
        t.commit();
    }

    // Create a PushService with a real ConnectionManager (no live sessions) for push-path tests.
    void SetUpPushService() {
        conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);
        push_service_ = std::make_unique<PushService>(conn_mgr_.get(), nullptr, msg_service_);
        ws_handler_ = std::make_unique<MessageWsHandler>(
            msg_service_, auth_mgr_, push_service_.get());
    }

    std::string make_token(const std::string& uid,
                           const std::string& device_id = "task8-device")
    {
        return auth_mgr_->generate_access_token(uid, uid + "-account", device_id, "web", 3600);
    }

    // Build a UnifiedMessage representing a WebSocket SendMessageRequest.
    std::unique_ptr<UnifiedMessage> make_send_message(
        const std::string& token,
        const std::string& header_from_uid,
        const std::string& header_to_uid,
        const std::string& content,
        const std::string& header_device_id = "task8-device",
        uint32_t cmd_id = im::command::CMD_SEND_MESSAGE,
        const std::string& protobuf_type = "im.message.SendMessageRequest")
    {
        auto msg = std::make_unique<UnifiedMessage>();

        im::base::IMHeader header;
        header.set_version("1.0");
        header.set_seq(42);
        header.set_cmd_id(cmd_id);
        header.set_from_uid(header_from_uid);
        header.set_to_uid(header_to_uid);
        header.set_token(token);
        header.set_device_id(header_device_id);
        header.set_platform("web");
        header.set_timestamp(static_cast<uint64_t>(now_ms()));
        msg->set_header(std::move(header));

        UnifiedMessage::SessionContext ctx;
        ctx.protocol = UnifiedMessage::Protocol::WEBSOCKET;
        ctx.session_id = "test-ws-session";
        ctx.receive_time = std::chrono::system_clock::now();
        msg->set_session_context(std::move(ctx));

        im::message::SendMessageRequest send_req;
        send_req.mutable_header()->set_from_uid(header_from_uid);
        send_req.mutable_header()->set_to_uid(header_to_uid);
        send_req.mutable_body()->set_content(content);
        send_req.mutable_body()->set_type(im::message::TEXT);

        std::string payload;
        send_req.SerializeToString(&payload);
        msg->set_protobuf_payload(payload);
        msg->set_protobuf_type_name(protobuf_type);

        return msg;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<MessageService> msg_service_;
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::unique_ptr<PushService> push_service_;
    std::unique_ptr<MessageWsHandler> ws_handler_;
    RecordingPushNotifier recording_notifier_;
};

// --- Existing Task 006 tests (constructor defaults nullptr for push params) ---

TEST_F(GatewayMessageWsTest, SendWithValidTokenPersistsAndReturnsResponse) {
    std::string token_user = "task8-test-valid-token-user";
    std::string receiver = "task8-test-valid-rec";
    std::string token = make_token(token_user);

    auto msg = make_send_message(token, token_user, receiver, "Hello WS!");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, 0) << "Error: " << result.error_message;
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(resp.message().content(), "Hello WS!");
    EXPECT_FALSE(resp.message().message_id().empty());
    EXPECT_EQ(resp.message().type(), im::message::TEXT);
    EXPECT_FALSE(resp.message().is_recalled());
    EXPECT_FALSE(resp.message().is_read());

    uint64_t msg_id = std::stoull(resp.message().message_id());
    EXPECT_GT(msg_id, 0);

    auto msgs = msg_service_->get_conversation(token_user, receiver, INT64_MAX, 10);
    bool found = false;
    for (const auto& m : msgs) {
        if (m.msg_id == msg_id) {
            EXPECT_EQ(m.sender_uid, token_user);
            EXPECT_EQ(m.receiver_uid, receiver);
            EXPECT_EQ(m.content, "Hello WS!");
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Persisted message not found in DB";
}

TEST_F(GatewayMessageWsTest, SuccessfulSendNotifiesReceiverThroughBoundary) {
    std::string sender = "task8-test-notify-sender";
    std::string receiver = "task8-test-notify-rec";
    std::string token = make_token(sender);
    ws_handler_ = std::make_unique<MessageWsHandler>(
        msg_service_, auth_mgr_, &recording_notifier_);

    auto msg = make_send_message(token, sender, receiver, "Notify receiver");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, 0) << "Error: " << result.error_message;

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    ASSERT_EQ(recording_notifier_.calls.size(), 1u);
    EXPECT_EQ(recording_notifier_.calls[0].receiver_uid, receiver);
    EXPECT_EQ(recording_notifier_.calls[0].msg_id,
              std::stoull(resp.message().message_id()));
    EXPECT_EQ(recording_notifier_.calls[0].content, "Notify receiver");
}

TEST_F(GatewayMessageWsTest, SenderIdentityFromTokenNotHeader) {
    std::string token_user = "task8-test-sender-from-token";
    std::string receiver = "task8-test-sender-rec";

    std::string real_token = make_token(token_user);

    std::string spoofed_sender = "task8-test-spoofed-sender";
    auto msg = make_send_message(real_token, spoofed_sender, receiver, "Spoofed sender test");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, 0) << "Error: " << result.error_message;

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    uint64_t msg_id = std::stoull(resp.message().message_id());

    auto msgs = msg_service_->get_conversation(token_user, receiver, INT64_MAX, 10);
    bool found = false;
    for (const auto& m : msgs) {
        if (m.msg_id == msg_id) {
            EXPECT_EQ(m.sender_uid, token_user) << "Sender must be token user, not " << spoofed_sender;
            EXPECT_NE(m.sender_uid, spoofed_sender);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Message not found in DB";
}

TEST_F(GatewayMessageWsTest, InvalidTokenReturnsAuthFailed) {
    std::string test_rec = "task8-test-invalid-token-rec";

    auto msg = make_send_message("invalid-token", "any-user", test_rec, "Bad token");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, im::base::ErrorCode::AUTH_FAILED);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::AUTH_FAILED);
}

TEST_F(GatewayMessageWsTest, DeviceIdMismatchReturnsAuthFailed) {
    std::string token_user = "task8-test-device-mismatch";
    std::string token = make_token(token_user, "token-device");

    auto msg = make_send_message(token, token_user, "rec", "Device mismatch",
                                  "wrong-device");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, im::base::ErrorCode::AUTH_FAILED);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::AUTH_FAILED);
}

TEST_F(GatewayMessageWsTest, MissingReceiverReturnsError) {
    std::string token_user = "task8-test-missing-rec";
    std::string token = make_token(token_user);

    auto msg = make_send_message(token, token_user, "", "No receiver");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, im::base::ErrorCode::PARAM_ERROR);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::PARAM_ERROR);
}

TEST_F(GatewayMessageWsTest, MissingContentReturnsError) {
    std::string token_user = "task8-test-missing-content";
    std::string token = make_token(token_user);

    auto msg = make_send_message(token, token_user, "rec", "");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, im::base::ErrorCode::PARAM_ERROR);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::PARAM_ERROR);
}

TEST_F(GatewayMessageWsTest, MalformedPayloadReturnsError) {
    std::string token_user = "task8-test-malformed";
    std::string token = make_token(token_user);

    auto msg = std::make_unique<UnifiedMessage>();

    im::base::IMHeader header;
    header.set_version("1.0");
    header.set_seq(1);
    header.set_cmd_id(im::command::CMD_SEND_MESSAGE);
    header.set_from_uid(token_user);
    header.set_to_uid("rec");
    header.set_token(token);
    header.set_device_id("task8-device");
    header.set_platform("web");
    header.set_timestamp(static_cast<uint64_t>(now_ms()));
    msg->set_header(std::move(header));

    UnifiedMessage::SessionContext ctx;
    ctx.protocol = UnifiedMessage::Protocol::WEBSOCKET;
    ctx.session_id = "test-session";
    ctx.receive_time = std::chrono::system_clock::now();
    msg->set_session_context(std::move(ctx));

    msg->set_protobuf_payload("not-a-valid-protobuf-message");
    msg->set_protobuf_type_name("im.message.SendMessageRequest");

    ProcessorResult result = ws_handler_->handle_send(*msg);
    EXPECT_EQ(result.status_code, im::base::ErrorCode::INVALID_REQUEST);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::INVALID_REQUEST);
}

TEST_F(GatewayMessageWsTest, NonWebSocketMessageReturnsError) {
    std::string token_user = "task8-test-non-ws";
    std::string token = make_token(token_user);

    auto msg = std::make_unique<UnifiedMessage>();

    im::base::IMHeader header;
    header.set_cmd_id(im::command::CMD_SEND_MESSAGE);
    header.set_from_uid(token_user);
    header.set_to_uid("rec");
    header.set_token(token);
    header.set_device_id("task8-device");
    msg->set_header(std::move(header));

    UnifiedMessage::SessionContext ctx;
    ctx.protocol = UnifiedMessage::Protocol::HTTP;
    ctx.session_id = "http-session";
    ctx.receive_time = std::chrono::system_clock::now();
    msg->set_session_context(std::move(ctx));

    im::message::SendMessageRequest send_req;
    send_req.mutable_body()->set_content("Hello from HTTP");
    std::string payload;
    send_req.SerializeToString(&payload);
    msg->set_protobuf_payload(payload);
    msg->set_protobuf_type_name("im.message.SendMessageRequest");

    ProcessorResult result = ws_handler_->handle_send(*msg);
    EXPECT_EQ(result.status_code, im::base::ErrorCode::INVALID_REQUEST);
    EXPECT_TRUE(result.protobuf_message.empty());
}

TEST_F(GatewayMessageWsTest, WrongProtobufTypeRejected) {
    std::string token_user = "task8-test-wrong-type";
    std::string token = make_token(token_user);

    auto msg = make_send_message(token, token_user, "rec", "Wrong type test",
                                  "task8-device",
                                  im::command::CMD_SEND_MESSAGE,
                                  "im.message.SomeOtherType");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, im::base::ErrorCode::INVALID_REQUEST);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::INVALID_REQUEST);

    auto msgs = msg_service_->get_conversation(token_user, "rec", INT64_MAX, 10);
    for (const auto& m : msgs) {
        EXPECT_NE(m.sender_uid, token_user)
            << "No message from " << token_user << " should have been persisted";
    }
}

TEST_F(GatewayMessageWsTest, WrongCmdIdRejected) {
    std::string token_user = "task8-test-wrong-cmd";
    std::string token = make_token(token_user);

    auto msg = make_send_message(token, token_user, "rec", "Wrong cmd",
                                  "task8-device",
                                  im::command::CMD_HEARTBEAT,
                                  "im.message.SendMessageRequest");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, im::base::ErrorCode::INVALID_REQUEST);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::INVALID_REQUEST);

    auto msgs = msg_service_->get_conversation(token_user, "rec", INT64_MAX, 10);
    for (const auto& m : msgs) {
        EXPECT_NE(m.sender_uid, token_user)
            << "No message from " << token_user << " should have been persisted";
    }
}

// --- Task 007: Online push-path tests ---

TEST_F(GatewayMessageWsTest, NullConnMgrSkipsPushGracefully) {
    // Default fixture: conn_mgr_ is nullptr, ws_server_ is nullptr.
    // Sending a message should succeed; push is silently skipped.
    std::string sender = "task8-test-null-push-sender";
    std::string receiver = "task8-test-null-push-rec";
    std::string token = make_token(sender);

    auto msg = make_send_message(token, sender, receiver, "No push infra");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, 0);

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::SUCCESS);

    // Message should NOT be marked delivered (no push happened).
    uint64_t msg_id = std::stoull(resp.message().message_id());
    auto msgs = msg_service_->get_conversation(sender, receiver, INT64_MAX, 10);
    for (const auto& m : msgs) {
        if (m.msg_id == msg_id) {
            EXPECT_EQ(m.delivered_time, 0) << "Message should not be marked delivered without push";
        }
    }
}

TEST_F(GatewayMessageWsTest, ConnMgrWithNoSessionsStaysUndelivered) {
    // Create handler with a real ConnectionManager but no live sessions.
    SetUpPushService();

    std::string sender = "task8-test-connmgr-sender";
    std::string receiver = "task8-test-connmgr-rec";
    std::string token = make_token(sender);

    auto msg = make_send_message(token, sender, receiver, "Push with no sessions");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, 0);

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::SUCCESS);

    // Receiver has no online sessions, so message should stay undelivered.
    uint64_t msg_id = std::stoull(resp.message().message_id());
    auto msgs = msg_service_->get_conversation(sender, receiver, INT64_MAX, 10);
    for (const auto& m : msgs) {
        if (m.msg_id == msg_id) {
            EXPECT_EQ(m.delivered_time, 0) << "Message should stay undelivered when receiver has no sessions";
        }
    }
}

} // anonymous namespace
