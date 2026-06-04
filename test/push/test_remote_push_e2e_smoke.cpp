#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <gateway/push/gateway_push_delivery_service.hpp>
#include <push_server_app.hpp>

#include "base.pb.h"
#include "command.pb.h"
#include "push.grpc.pb.h"
#include "push.pb.h"
#include "../../common/network/protobuf_codec.hpp"

namespace {

using im::gateway::GatewayPushDeliveryService;
using im::network::ProtobufCodec;
using im::service::push::PushDeliveryMarker;
using im::service::push::PushPayloadSender;
using im::service::push::PushServerApp;
using im::service::push::PushServerConfig;
using im::service::push::PushSessionInfo;
using im::service::push::PushSessionProvider;

class RecordingSessionProvider final : public PushSessionProvider {
public:
    std::vector<PushSessionInfo> get_sessions(const std::string& receiver_uid) override {
        requested_uids.push_back(receiver_uid);
        return sessions;
    }

    std::vector<PushSessionInfo> sessions;
    std::vector<std::string> requested_uids;
};

class RecordingPayloadSender final : public PushPayloadSender {
public:
    bool send_payload(const std::string& session_id, const std::string& payload) override {
        session_ids.push_back(session_id);
        payloads.push_back(payload);
        return accepted;
    }

    bool accepted = true;
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

TEST(RemotePushEndToEndSmokeTest, NotifyUserReachesGatewayDeliveryEndpoint) {
    RecordingSessionProvider sessions;
    RecordingPayloadSender payload_sender;
    RecordingDeliveryMarker delivery_marker;

    sessions.sessions.push_back({
        .session_id = "session-web-1",
        .platform = "web",
        .connect_time = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(123456)),
    });

    GatewayDeliveryEndpoint gateway_endpoint(
        &sessions, &payload_sender, &delivery_marker);
    ASSERT_TRUE(gateway_endpoint.is_running());

    PushServerConfig config;
    config.listen_address = "127.0.0.1:0";
    config.gateway_delivery_endpoint = gateway_endpoint.endpoint();
    config.timeout_ms = 1000;

    PushServerApp push_server(config);
    ASSERT_TRUE(push_server.start());
    ASSERT_GT(push_server.selected_port(), 0);

    const std::string push_endpoint =
        "127.0.0.1:" + std::to_string(push_server.selected_port());
    auto channel = ::grpc::CreateChannel(push_endpoint, ::grpc::InsecureChannelCredentials());
    auto stub = im::push::PushService::NewStub(channel);

    im::push::NotifyUserRequest request;
    request.set_receiver_uid("receiver-remote");
    request.set_msg_id(9001);
    request.set_content("remote hello");

    im::push::NotifyUserResponse response;
    ::grpc::ClientContext context;
    auto status = stub->NotifyUser(&context, request, &response);

    EXPECT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(response.base().error_code(), im::base::SUCCESS);

    ASSERT_EQ(sessions.requested_uids.size(), 1u);
    EXPECT_EQ(sessions.requested_uids[0], "receiver-remote");

    ASSERT_EQ(payload_sender.session_ids.size(), 1u);
    EXPECT_EQ(payload_sender.session_ids[0], "session-web-1");
    ASSERT_EQ(payload_sender.payloads.size(), 1u);
    EXPECT_FALSE(payload_sender.payloads[0].empty());

    im::base::IMHeader decoded_header;
    im::push::PushRequest decoded_push;
    ASSERT_TRUE(ProtobufCodec::decode(
        payload_sender.payloads[0], decoded_header, decoded_push));
    EXPECT_EQ(decoded_header.cmd_id(), im::command::CMD_PUSH_MESSAGE);
    EXPECT_EQ(decoded_header.to_uid(), "receiver-remote");
    ASSERT_TRUE(decoded_push.has_body());
    EXPECT_EQ(decoded_push.body().type(), im::push::PUSH_MESSAGE);
    EXPECT_EQ(decoded_push.body().content(), "remote hello");
    EXPECT_EQ(decoded_push.body().related_message_id(), "9001");

    ASSERT_EQ(delivery_marker.msg_ids.size(), 1u);
    EXPECT_EQ(delivery_marker.msg_ids[0], 9001u);
    ASSERT_EQ(delivery_marker.delivered_times.size(), 1u);
    EXPECT_GT(delivery_marker.delivered_times[0], 0);

    push_server.shutdown();
}

TEST(RemotePushEndToEndSmokeTest, NoGatewaySessionsLeavesMessageUndelivered) {
    RecordingSessionProvider sessions;
    RecordingPayloadSender payload_sender;
    RecordingDeliveryMarker delivery_marker;

    GatewayDeliveryEndpoint gateway_endpoint(
        &sessions, &payload_sender, &delivery_marker);
    ASSERT_TRUE(gateway_endpoint.is_running());

    PushServerConfig config;
    config.listen_address = "127.0.0.1:0";
    config.gateway_delivery_endpoint = gateway_endpoint.endpoint();
    config.timeout_ms = 1000;

    PushServerApp push_server(config);
    ASSERT_TRUE(push_server.start());

    const std::string push_endpoint =
        "127.0.0.1:" + std::to_string(push_server.selected_port());
    auto channel = ::grpc::CreateChannel(push_endpoint, ::grpc::InsecureChannelCredentials());
    auto stub = im::push::PushService::NewStub(channel);

    im::push::NotifyUserRequest request;
    request.set_receiver_uid("offline-user");
    request.set_msg_id(9002);
    request.set_content("offline hello");

    im::push::NotifyUserResponse response;
    ::grpc::ClientContext context;
    auto status = stub->NotifyUser(&context, request, &response);

    EXPECT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(response.base().error_code(), im::base::SUCCESS);

    ASSERT_EQ(sessions.requested_uids.size(), 1u);
    EXPECT_EQ(sessions.requested_uids[0], "offline-user");
    EXPECT_TRUE(payload_sender.session_ids.empty());
    EXPECT_TRUE(delivery_marker.msg_ids.empty());

    push_server.shutdown();
}

} // namespace
