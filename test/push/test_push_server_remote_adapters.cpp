#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/support/status.h>
#include <push_server_adapters.hpp>

#include "base.pb.h"
#include "push.pb.h"

namespace {

using im::service::push::GatewayDeliveryRpcClient;
using im::service::push::RemoteGatewayPushDeliveryMarker;
using im::service::push::RemoteGatewayPushPayloadSender;
using im::service::push::RemoteGatewayPushSessionProvider;

class FakeGatewayDeliveryRpcClient : public GatewayDeliveryRpcClient {
public:
    ::grpc::Status list_user_sessions(
        ::grpc::ClientContext* /*context*/,
        const im::push::ListUserSessionsRequest& request,
        im::push::ListUserSessionsResponse* response) override {
        list_requests.push_back(request);
        response->mutable_base()->set_error_code(list_base_code);
        response->mutable_base()->set_error_message(list_base_message);
        for (const auto& session : list_sessions) {
            auto* out = response->add_sessions();
            out->CopyFrom(session);
        }
        return list_status;
    }

    ::grpc::Status send_session_payload(
        ::grpc::ClientContext* /*context*/,
        const im::push::SendSessionPayloadRequest& request,
        im::push::SendSessionPayloadResponse* response) override {
        send_requests.push_back(request);
        response->mutable_base()->set_error_code(send_base_code);
        response->mutable_base()->set_error_message(send_base_message);
        response->set_accepted(send_accepted);
        return send_status;
    }

    ::grpc::Status mark_message_delivered(
        ::grpc::ClientContext* /*context*/,
        const im::push::MarkMessageDeliveredRequest& request,
        im::push::MarkMessageDeliveredResponse* response) override {
        mark_requests.push_back(request);
        response->mutable_base()->set_error_code(mark_base_code);
        response->mutable_base()->set_error_message(mark_base_message);
        response->set_marked(mark_marked);
        return mark_status;
    }

    std::vector<im::push::ListUserSessionsRequest> list_requests;
    std::vector<im::push::SendSessionPayloadRequest> send_requests;
    std::vector<im::push::MarkMessageDeliveredRequest> mark_requests;

    std::vector<im::push::PushSession> list_sessions;

    ::grpc::Status list_status = ::grpc::Status::OK;
    ::grpc::Status send_status = ::grpc::Status::OK;
    ::grpc::Status mark_status = ::grpc::Status::OK;

    im::base::ErrorCode list_base_code = im::base::SUCCESS;
    im::base::ErrorCode send_base_code = im::base::SUCCESS;
    im::base::ErrorCode mark_base_code = im::base::SUCCESS;

    std::string list_base_message;
    std::string send_base_message;
    std::string mark_base_message;

    bool send_accepted = true;
    bool mark_marked = true;
};

TEST(PushServerRemoteAdaptersTest, SessionProviderMapsGatewaySessions) {
    auto fake = std::make_shared<FakeGatewayDeliveryRpcClient>();
    im::push::PushSession session;
    session.set_session_id("session-1");
    session.set_platform("ios");
    session.set_connect_time_ms(12345);
    fake->list_sessions.push_back(session);

    RemoteGatewayPushSessionProvider provider(fake);

    auto sessions = provider.get_sessions("user-1");

    ASSERT_EQ(fake->list_requests.size(), 1u);
    EXPECT_EQ(fake->list_requests[0].receiver_uid(), "user-1");
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions[0].session_id, "session-1");
    EXPECT_EQ(sessions[0].platform, "ios");
    const auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        sessions[0].connect_time.time_since_epoch()).count();
    EXPECT_EQ(epoch_ms, 12345);
}

TEST(PushServerRemoteAdaptersTest, SessionProviderReturnsEmptyOnRpcFailure) {
    auto fake = std::make_shared<FakeGatewayDeliveryRpcClient>();
    fake->list_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteGatewayPushSessionProvider provider(fake);

    auto sessions = provider.get_sessions("user-2");

    EXPECT_TRUE(sessions.empty());
    ASSERT_EQ(fake->list_requests.size(), 1u);
}

TEST(PushServerRemoteAdaptersTest, SessionProviderReturnsEmptyOnBaseError) {
    auto fake = std::make_shared<FakeGatewayDeliveryRpcClient>();
    fake->list_base_code = im::base::SERVER_ERROR;
    fake->list_base_message = "gateway failed";
    RemoteGatewayPushSessionProvider provider(fake);

    auto sessions = provider.get_sessions("user-3");

    EXPECT_TRUE(sessions.empty());
    ASSERT_EQ(fake->list_requests.size(), 1u);
}

TEST(PushServerRemoteAdaptersTest, PayloadSenderSendsExpectedRequest) {
    auto fake = std::make_shared<FakeGatewayDeliveryRpcClient>();
    RemoteGatewayPushPayloadSender sender(fake);

    const bool accepted = sender.send_payload("session-2", "payload");

    EXPECT_TRUE(accepted);
    ASSERT_EQ(fake->send_requests.size(), 1u);
    EXPECT_EQ(fake->send_requests[0].session_id(), "session-2");
    EXPECT_EQ(fake->send_requests[0].payload(), "payload");
}

TEST(PushServerRemoteAdaptersTest, PayloadSenderReturnsFalseWhenGatewayRejects) {
    auto fake = std::make_shared<FakeGatewayDeliveryRpcClient>();
    fake->send_accepted = false;
    RemoteGatewayPushPayloadSender sender(fake);

    EXPECT_FALSE(sender.send_payload("session-3", "payload"));
}

TEST(PushServerRemoteAdaptersTest, PayloadSenderReturnsFalseOnBaseError) {
    auto fake = std::make_shared<FakeGatewayDeliveryRpcClient>();
    fake->send_base_code = im::base::SERVER_ERROR;
    RemoteGatewayPushPayloadSender sender(fake);

    EXPECT_FALSE(sender.send_payload("session-4", "payload"));
}

TEST(PushServerRemoteAdaptersTest, DeliveryMarkerSendsExpectedRequest) {
    auto fake = std::make_shared<FakeGatewayDeliveryRpcClient>();
    RemoteGatewayPushDeliveryMarker marker(fake);

    const bool marked = marker.mark_delivered(88, 9999);

    EXPECT_TRUE(marked);
    ASSERT_EQ(fake->mark_requests.size(), 1u);
    EXPECT_EQ(fake->mark_requests[0].msg_id(), 88u);
    EXPECT_EQ(fake->mark_requests[0].delivered_time(), 9999);
}

TEST(PushServerRemoteAdaptersTest, DeliveryMarkerReturnsFalseOnTransportFailure) {
    auto fake = std::make_shared<FakeGatewayDeliveryRpcClient>();
    fake->mark_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteGatewayPushDeliveryMarker marker(fake);

    EXPECT_FALSE(marker.mark_delivered(89, 10000));
    ASSERT_EQ(fake->mark_requests.size(), 1u);
}

} // namespace
