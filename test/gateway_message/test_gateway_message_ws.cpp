#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <message.hpp>
#include <message-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/message_ws_handler.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
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
using im::gateway::MessageWsHandler;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::ProcessorResult;
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
        ws_handler_ = std::make_unique<MessageWsHandler>(msg_service_, auth_mgr_);
    }

    void TearDown() override {
        CleanupTestData();
        ws_handler_.reset();
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
            WHERE "sender_uid" LIKE 'task6-test-%'
               OR "receiver_uid" LIKE 'task6-test-%')");
        t.commit();
    }

    std::string make_token(const std::string& uid,
                           const std::string& device_id = "task6-device")
    {
        return auth_mgr_->generate_access_token(uid, uid + "-account", device_id, "web", 3600);
    }

    // Build a UnifiedMessage representing a WebSocket SendMessageRequest.
    // sender_uid and device_id in the header MAY differ from the token's values
    // to test that the handler trusts the token, not the header.
    std::unique_ptr<UnifiedMessage> make_send_message(
        const std::string& token,
        const std::string& header_from_uid,
        const std::string& header_to_uid,
        const std::string& content,
        const std::string& header_device_id = "task6-device",
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
    std::unique_ptr<MessageWsHandler> ws_handler_;
};

TEST_F(GatewayMessageWsTest, SendWithValidTokenPersistsAndReturnsResponse) {
    std::string token_user = "task6-test-valid-token-user";
    std::string receiver = "task6-test-valid-rec";
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

TEST_F(GatewayMessageWsTest, SenderIdentityFromTokenNotHeader) {
    std::string token_user = "task6-test-sender-from-token";
    std::string receiver = "task6-test-sender-rec";

    // Generate a token for token_user.
    std::string real_token = make_token(token_user);

    // Build a message where header.from_uid claims to be a different user.
    std::string spoofed_sender = "task6-test-spoofed-sender";
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
    std::string test_rec = "task6-test-invalid-token-rec";

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
    std::string token_user = "task6-test-device-mismatch";
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
    std::string token_user = "task6-test-missing-rec";
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
    std::string token_user = "task6-test-missing-content";
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
    std::string token_user = "task6-test-malformed";
    std::string token = make_token(token_user);

    auto msg = std::make_unique<UnifiedMessage>();

    im::base::IMHeader header;
    header.set_version("1.0");
    header.set_seq(1);
    header.set_cmd_id(im::command::CMD_SEND_MESSAGE);
    header.set_from_uid(token_user);
    header.set_to_uid("rec");
    header.set_token(token);
    header.set_device_id("task6-device");
    header.set_platform("web");
    header.set_timestamp(static_cast<uint64_t>(now_ms()));
    msg->set_header(std::move(header));

    UnifiedMessage::SessionContext ctx;
    ctx.protocol = UnifiedMessage::Protocol::WEBSOCKET;
    ctx.session_id = "test-session";
    ctx.receive_time = std::chrono::system_clock::now();
    msg->set_session_context(std::move(ctx));

    // Set payload to garbage bytes that won't parse as SendMessageRequest.
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
    std::string token_user = "task6-test-non-ws";
    std::string token = make_token(token_user);

    auto msg = std::make_unique<UnifiedMessage>();

    im::base::IMHeader header;
    header.set_cmd_id(im::command::CMD_SEND_MESSAGE);
    header.set_from_uid(token_user);
    header.set_to_uid("rec");
    header.set_token(token);
    header.set_device_id("task6-device");
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
    // Non-WebSocket check returns empty protobuf_message (gateway-level check).
    EXPECT_TRUE(result.protobuf_message.empty());
}

TEST_F(GatewayMessageWsTest, WrongProtobufTypeRejected) {
    std::string token_user = "task6-test-wrong-type";
    std::string token = make_token(token_user);

    auto msg = make_send_message(token, token_user, "rec", "Wrong type test",
                                  "task6-device",
                                  im::command::CMD_SEND_MESSAGE,
                                  "im.message.SomeOtherType");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, im::base::ErrorCode::INVALID_REQUEST);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::INVALID_REQUEST);

    // Verify no message was persisted.
    auto msgs = msg_service_->get_conversation(token_user, "rec", INT64_MAX, 10);
    for (const auto& m : msgs) {
        EXPECT_NE(m.sender_uid, token_user)
            << "No message from " << token_user << " should have been persisted";
    }
}

TEST_F(GatewayMessageWsTest, WrongCmdIdRejected) {
    std::string token_user = "task6-test-wrong-cmd";
    std::string token = make_token(token_user);

    // Use CMD_HEARTBEAT instead of CMD_SEND_MESSAGE.
    auto msg = make_send_message(token, token_user, "rec", "Wrong cmd",
                                  "task6-device",
                                  im::command::CMD_HEARTBEAT,
                                  "im.message.SendMessageRequest");
    ProcessorResult result = ws_handler_->handle_send(*msg);

    EXPECT_EQ(result.status_code, im::base::ErrorCode::INVALID_REQUEST);
    EXPECT_FALSE(result.protobuf_message.empty());

    im::base::IMHeader resp_header;
    im::message::SendMessageResponse resp;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, resp_header, resp));
    EXPECT_EQ(resp.base().error_code(), im::base::INVALID_REQUEST);

    // Verify no message was persisted.
    auto msgs = msg_service_->get_conversation(token_user, "rec", INT64_MAX, 10);
    for (const auto& m : msgs) {
        EXPECT_NE(m.sender_uid, token_user)
            << "No message from " << token_user << " should have been persisted";
    }
}

} // anonymous namespace
