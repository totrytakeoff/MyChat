#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <database/redis/redis_mgr.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <gateway/http/group_message_http_controller.hpp>
#include <gateway/push/gateway_push_delivery_service.hpp>
#include <gateway/push/remote_push_notifier.hpp>
#include <gateway/ws/message_ws_handler.hpp>
#include <group_message_service.hpp>
#include <group_service.hpp>
#include <message_service.hpp>
#include <password_hasher.hpp>
#include <push_server_app.hpp>
#include <user_service.hpp>
#include <utils/log_manager.hpp>

#include "../support/postgres_schema.hpp"

#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/proto/command.pb.h"
#include "../../common/proto/message.pb.h"
#include "../../common/proto/push.pb.h"

namespace {

using json = nlohmann::json;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::GatewayPushDeliveryService;
using im::gateway::GroupMessageHttpController;
using im::gateway::MessageWsHandler;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::ProcessorResult;
using im::gateway::RemotePushNotifier;
using im::gateway::UnifiedMessage;
using im::network::ProtobufCodec;
using im::service::group::CreateGroupRequest;
using im::service::group::GroupMessageService;
using im::service::group::GroupService;
using im::service::push::PushDeliveryMarker;
using im::service::push::PushPayloadSender;
using im::service::push::PushServerApp;
using im::service::push::PushServerConfig;
using im::service::push::PushSessionInfo;
using im::service::push::PushSessionProvider;
using im::service::user::PasswordHasher;
using im::service::user::RegisterRequest;
using im::service::user::UserService;
using im::utils::LogManager;

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

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

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class RecordingSessionProvider final : public PushSessionProvider {
public:
    std::vector<PushSessionInfo> get_sessions(const std::string& receiver_uid) override {
        requested_uids.push_back(receiver_uid);
        auto it = sessions_by_uid.find(receiver_uid);
        if (it == sessions_by_uid.end()) {
            return {};
        }
        return it->second;
    }

    void add_session(const std::string& uid,
                     const std::string& session_id,
                     const std::string& platform = "web") {
        sessions_by_uid[uid].push_back({
            .session_id = session_id,
            .platform = platform,
            .connect_time = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(123456)),
        });
    }

    std::vector<std::string> requested_uids;
    std::unordered_map<std::string, std::vector<PushSessionInfo>> sessions_by_uid;
};

class RecordingPayloadSender final : public PushPayloadSender {
public:
    bool send_payload(const std::string& session_id, const std::string& payload) override {
        session_ids.push_back(session_id);
        payloads.push_back(payload);
        return true;
    }

    std::vector<std::string> session_ids;
    std::vector<std::string> payloads;
};

class RecordingDeliveryMarker final : public PushDeliveryMarker {
public:
    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override {
        msg_ids.push_back(msg_id);
        delivered_times.push_back(delivered_time);
        return true;
    }

    std::vector<uint64_t> msg_ids;
    std::vector<int64_t> delivered_times;
};

class GatewayDeliveryEndpoint {
public:
    GatewayDeliveryEndpoint(PushSessionProvider* session_provider,
                            PushPayloadSender* payload_sender,
                            PushDeliveryMarker* delivery_marker)
        : service_(session_provider, payload_sender, delivery_marker) {
        ::grpc::ServerBuilder builder;
        builder.AddListeningPort("127.0.0.1:0",
                                 ::grpc::InsecureServerCredentials(),
                                 &selected_port_);
        builder.RegisterService(&service_);
        server_ = builder.BuildAndStart();
    }

    ~GatewayDeliveryEndpoint() {
        if (server_) {
            server_->Shutdown();
        }
    }

    bool is_running() const { return server_ != nullptr; }
    std::string endpoint() const {
        return "127.0.0.1:" + std::to_string(selected_port_);
    }

private:
    GatewayPushDeliveryService service_;
    std::unique_ptr<::grpc::Server> server_;
    int selected_port_ = 0;
};

class RemotePushGatewayEntrypointsTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("remote_push_gateway_entrypoints");
        LogManager::SetLogToConsole("auth_mgr");
        LogManager::SetLogToConsole("redis_manager");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        im::test::EnsureCoreSchema(*db_);
        CleanupTestData();

        auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(
            "test_secret_key_for_remote_push_gateway_entrypoints", config_path());
        user_svc_ = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
        msg_svc_ = std::make_shared<im::service::message::MessageService>(db_);
        group_svc_ = std::make_shared<GroupService>(db_, user_svc_);
        group_msg_svc_ = std::make_shared<GroupMessageService>(db_, user_svc_, group_svc_);

        gateway_endpoint_ = std::make_unique<GatewayDeliveryEndpoint>(
            &sessions_, &payload_sender_, &delivery_marker_);
        ASSERT_TRUE(gateway_endpoint_->is_running());

        PushServerConfig config;
        config.listen_address = "127.0.0.1:0";
        config.gateway_delivery_endpoint = gateway_endpoint_->endpoint();
        config.timeout_ms = 1000;
        push_server_ = std::make_unique<PushServerApp>(config);
        ASSERT_TRUE(push_server_->start());
        ASSERT_GT(push_server_->selected_port(), 0);

        remote_notifier_ = std::make_unique<RemotePushNotifier>(
            "127.0.0.1:" + std::to_string(push_server_->selected_port()),
            std::chrono::milliseconds(1000));
    }

    void TearDown() override {
        remote_notifier_.reset();
        if (push_server_) {
            push_server_->shutdown();
        }
        push_server_.reset();
        gateway_endpoint_.reset();
        CleanupTestData();
        group_msg_svc_.reset();
        group_svc_.reset();
        msg_svc_.reset();
        user_svc_.reset();
        auth_mgr_.reset();
        db_.reset();
        redis_manager().shutdown();
    }

    void CleanupTestData() {
        if (!db_) {
            return;
        }
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_group_messages"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups"
                    WHERE "name" LIKE 'remote-push-entry-%'
                )
               OR "sender_uid" IN (
                    SELECT "uid" FROM "im_users"
                    WHERE "account" LIKE 'remote-push-entry-%'
                ))");
        db_->execute(R"(DELETE FROM "im_group_members"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups"
                    WHERE "name" LIKE 'remote-push-entry-%'
                )
               OR "user_uid" IN (
                    SELECT "uid" FROM "im_users"
                    WHERE "account" LIKE 'remote-push-entry-%'
                ))");
        db_->execute(R"(DELETE FROM "im_groups"
            WHERE "creator_uid" IN (
                    SELECT "uid" FROM "im_users"
                    WHERE "account" LIKE 'remote-push-entry-%'
                )
               OR "name" LIKE 'remote-push-entry-%')");
        db_->execute(R"(DELETE FROM "im_messages"
            WHERE "sender_uid" LIKE 'remote-push-entry-%'
               OR "receiver_uid" LIKE 'remote-push-entry-%')");
        db_->execute(R"(DELETE FROM "im_users"
            WHERE "account" LIKE 'remote-push-entry-%')");
        t.commit();
    }

    std::string make_token(const std::string& uid,
                           const std::string& device_id = "remote-entry-device") {
        return auth_mgr_->generate_access_token(uid, uid + "-acct", device_id, "web", 3600);
    }

    std::string create_user(const std::string& account) {
        RegisterRequest req;
        req.account = account;
        req.password = "testPass123";
        req.nickname = account;
        req.now_ms = kNowMs;
        auto result = user_svc_->register_user(req);
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

    std::unique_ptr<UnifiedMessage> make_send_message(const std::string& token,
                                                      const std::string& sender,
                                                      const std::string& receiver,
                                                      const std::string& content) {
        auto msg = std::make_unique<UnifiedMessage>();

        im::base::IMHeader header;
        header.set_version("1.0");
        header.set_seq(42);
        header.set_cmd_id(im::command::CMD_SEND_MESSAGE);
        header.set_from_uid(sender);
        header.set_to_uid(receiver);
        header.set_token(token);
        header.set_device_id("remote-entry-device");
        header.set_platform("web");
        header.set_timestamp(static_cast<uint64_t>(now_ms()));
        msg->set_header(std::move(header));

        UnifiedMessage::SessionContext ctx;
        ctx.protocol = UnifiedMessage::Protocol::WEBSOCKET;
        ctx.session_id = "remote-entry-ws-session";
        ctx.receive_time = std::chrono::system_clock::now();
        msg->set_session_context(std::move(ctx));

        im::message::SendMessageRequest send_req;
        send_req.mutable_header()->set_from_uid(sender);
        send_req.mutable_header()->set_to_uid(receiver);
        send_req.mutable_body()->set_content(content);
        send_req.mutable_body()->set_type(im::message::TEXT);

        std::string payload;
        send_req.SerializeToString(&payload);
        msg->set_protobuf_payload(payload);
        msg->set_protobuf_type_name("im.message.SendMessageRequest");

        return msg;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<UserService> user_svc_;
    std::shared_ptr<im::service::message::MessageService> msg_svc_;
    std::shared_ptr<GroupService> group_svc_;
    std::shared_ptr<GroupMessageService> group_msg_svc_;
    RecordingSessionProvider sessions_;
    RecordingPayloadSender payload_sender_;
    RecordingDeliveryMarker delivery_marker_;
    std::unique_ptr<GatewayDeliveryEndpoint> gateway_endpoint_;
    std::unique_ptr<PushServerApp> push_server_;
    std::unique_ptr<RemotePushNotifier> remote_notifier_;
};

TEST_F(RemotePushGatewayEntrypointsTest, DirectMessageWsUsesRemotePushPath) {
    const std::string sender = "remote-push-entry-direct-sender";
    const std::string receiver = "remote-push-entry-direct-receiver";
    sessions_.add_session(receiver, "direct-session-1");

    MessageWsHandler handler(msg_svc_, auth_mgr_, remote_notifier_.get());
    auto msg = make_send_message(
        make_token(sender), sender, receiver, "direct remote payload");

    ProcessorResult result = handler.handle_send(*msg);

    ASSERT_EQ(result.status_code, 0) << result.error_message;

    im::base::IMHeader response_header;
    im::message::SendMessageResponse response;
    ASSERT_TRUE(ProtobufCodec::decode(result.protobuf_message, response_header, response));
    ASSERT_EQ(response.base().error_code(), im::base::SUCCESS);
    const uint64_t msg_id = std::stoull(response.message().message_id());

    ASSERT_EQ(sessions_.requested_uids.size(), 1u);
    EXPECT_EQ(sessions_.requested_uids[0], receiver);
    ASSERT_EQ(payload_sender_.session_ids.size(), 1u);
    EXPECT_EQ(payload_sender_.session_ids[0], "direct-session-1");
    ASSERT_EQ(delivery_marker_.msg_ids.size(), 1u);
    EXPECT_EQ(delivery_marker_.msg_ids[0], msg_id);

    im::base::IMHeader push_header;
    im::push::PushRequest push_request;
    ASSERT_TRUE(ProtobufCodec::decode(payload_sender_.payloads[0], push_header, push_request));
    EXPECT_EQ(push_header.cmd_id(), im::command::CMD_PUSH_MESSAGE);
    EXPECT_EQ(push_header.to_uid(), receiver);
    ASSERT_TRUE(push_request.has_body());
    EXPECT_EQ(push_request.body().content(), "direct remote payload");
    EXPECT_EQ(push_request.body().related_message_id(), std::to_string(msg_id));
}

TEST_F(RemotePushGatewayEntrypointsTest, GroupMessageHttpUsesRemotePushForMembersOnly) {
    const std::string owner = create_user("remote-push-entry-group-owner");
    const std::string member1 = create_user("remote-push-entry-group-member1");
    const std::string member2 = create_user("remote-push-entry-group-member2");
    const uint64_t group_id = create_group("remote-push-entry-group", owner);
    ASSERT_TRUE(group_svc_->join_group(group_id, member1, kNowMs).ok);
    ASSERT_TRUE(group_svc_->join_group(group_id, member2, kNowMs).ok);

    sessions_.add_session(member1, "group-session-1");
    sessions_.add_session(member2, "group-session-2");
    sessions_.add_session(owner, "owner-should-not-receive");

    GroupMessageHttpController controller(
        group_svc_, group_msg_svc_, auth_mgr_, remote_notifier_.get());

    httplib::Request request;
    request.method = "POST";
    request.headers.emplace("Authorization", "Bearer " + make_token(owner));
    request.body = json{{"group_id", group_id}, {"content", "group remote payload"}}.dump();
    httplib::Response response;

    controller.handle_send_message(request, response);

    ASSERT_EQ(response.status, 201);
    const auto body = json::parse(response.body);
    const uint64_t msg_id = body["msg_id"].get<uint64_t>();

    ASSERT_EQ(sessions_.requested_uids.size(), 2u);
    EXPECT_NE(std::find(sessions_.requested_uids.begin(),
                        sessions_.requested_uids.end(),
                        member1),
              sessions_.requested_uids.end());
    EXPECT_NE(std::find(sessions_.requested_uids.begin(),
                        sessions_.requested_uids.end(),
                        member2),
              sessions_.requested_uids.end());
    EXPECT_EQ(std::find(sessions_.requested_uids.begin(),
                        sessions_.requested_uids.end(),
                        owner),
              sessions_.requested_uids.end());

    ASSERT_EQ(payload_sender_.session_ids.size(), 2u);
    EXPECT_NE(std::find(payload_sender_.session_ids.begin(),
                        payload_sender_.session_ids.end(),
                        "group-session-1"),
              payload_sender_.session_ids.end());
    EXPECT_NE(std::find(payload_sender_.session_ids.begin(),
                        payload_sender_.session_ids.end(),
                        "group-session-2"),
              payload_sender_.session_ids.end());
    EXPECT_EQ(std::find(payload_sender_.session_ids.begin(),
                        payload_sender_.session_ids.end(),
                        "owner-should-not-receive"),
              payload_sender_.session_ids.end());

    ASSERT_EQ(delivery_marker_.msg_ids.size(), 2u);
    EXPECT_TRUE(std::all_of(delivery_marker_.msg_ids.begin(),
                            delivery_marker_.msg_ids.end(),
                            [msg_id](uint64_t marked_id) {
                                return marked_id == msg_id;
                            }));

    ASSERT_EQ(payload_sender_.payloads.size(), 2u);
    for (const auto& payload : payload_sender_.payloads) {
        im::base::IMHeader push_header;
        im::push::PushRequest push_request;
        ASSERT_TRUE(ProtobufCodec::decode(payload, push_header, push_request));
        EXPECT_EQ(push_header.cmd_id(), im::command::CMD_PUSH_MESSAGE);
        ASSERT_TRUE(push_request.has_body());
        EXPECT_EQ(push_request.body().content(), "group remote payload");
        EXPECT_EQ(push_request.body().related_message_id(), std::to_string(msg_id));
    }
}

} // namespace
