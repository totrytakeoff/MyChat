#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <gateway/push/gateway_push_delivery_service.hpp>

#include "base.pb.h"
#include "push.pb.h"

namespace {

using im::gateway::GatewayPushDeliveryService;
using im::service::push::PushDeliveryMarker;
using im::service::push::PushPayloadSender;
using im::service::push::PushSessionInfo;
using im::service::push::PushSessionProvider;

class FakeSessionProvider : public PushSessionProvider {
public:
    std::vector<PushSessionInfo> get_sessions(const std::string& receiver_uid) override {
        requested_uids.push_back(receiver_uid);
        if (throw_on_call) {
            throw std::runtime_error("session lookup failed");
        }
        return sessions;
    }

    std::vector<PushSessionInfo> sessions;
    std::vector<std::string> requested_uids;
    bool throw_on_call = false;
};

class FakePayloadSender : public PushPayloadSender {
public:
    bool send_payload(const std::string& session_id, const std::string& payload) override {
        sent_session_ids.push_back(session_id);
        sent_payloads.push_back(payload);
        if (throw_on_call) {
            throw std::runtime_error("payload send failed");
        }
        return accepted;
    }

    bool accepted = true;
    bool throw_on_call = false;
    std::vector<std::string> sent_session_ids;
    std::vector<std::string> sent_payloads;
};

class FakeDeliveryMarker : public PushDeliveryMarker {
public:
    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override {
        marked_msg_ids.push_back(msg_id);
        marked_times.push_back(delivered_time);
        if (throw_on_call) {
            throw std::runtime_error("mark failed");
        }
        return marked;
    }

    bool marked = true;
    bool throw_on_call = false;
    std::vector<uint64_t> marked_msg_ids;
    std::vector<int64_t> marked_times;
};

int64_t epoch_ms(const std::chrono::system_clock::time_point& time_point) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch()).count();
}

TEST(GatewayPushDeliveryServiceTest, ListUserSessionsDelegatesAndMapsSessions) {
    FakeSessionProvider sessions;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;

    const auto connect_time =
        std::chrono::system_clock::time_point(std::chrono::milliseconds(123456));
    sessions.sessions.push_back({
        .session_id = "session-1",
        .platform = "web",
        .connect_time = connect_time,
    });

    GatewayPushDeliveryService service(&sessions, &sender, &marker);

    im::push::ListUserSessionsRequest request;
    request.set_receiver_uid("user-1");
    im::push::ListUserSessionsResponse response;

    auto status = service.ListUserSessions(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::SUCCESS);
    ASSERT_EQ(sessions.requested_uids.size(), 1u);
    EXPECT_EQ(sessions.requested_uids[0], "user-1");
    ASSERT_EQ(response.sessions_size(), 1);
    EXPECT_EQ(response.sessions(0).session_id(), "session-1");
    EXPECT_EQ(response.sessions(0).platform(), "web");
    EXPECT_EQ(response.sessions(0).connect_time_ms(), epoch_ms(connect_time));
}

TEST(GatewayPushDeliveryServiceTest, ListUserSessionsRejectsMissingReceiver) {
    FakeSessionProvider sessions;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    GatewayPushDeliveryService service(&sessions, &sender, &marker);

    im::push::ListUserSessionsRequest request;
    im::push::ListUserSessionsResponse response;

    auto status = service.ListUserSessions(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::PARAM_ERROR);
    EXPECT_TRUE(sessions.requested_uids.empty());
}

TEST(GatewayPushDeliveryServiceTest, SendSessionPayloadDelegatesAndReturnsAccepted) {
    FakeSessionProvider sessions;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    GatewayPushDeliveryService service(&sessions, &sender, &marker);

    im::push::SendSessionPayloadRequest request;
    request.set_session_id("session-2");
    request.set_payload("encoded");
    im::push::SendSessionPayloadResponse response;

    auto status = service.SendSessionPayload(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::SUCCESS);
    EXPECT_TRUE(response.accepted());
    ASSERT_EQ(sender.sent_session_ids.size(), 1u);
    EXPECT_EQ(sender.sent_session_ids[0], "session-2");
    EXPECT_EQ(sender.sent_payloads[0], "encoded");
}

TEST(GatewayPushDeliveryServiceTest, SendSessionPayloadRejectsMissingPayload) {
    FakeSessionProvider sessions;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    GatewayPushDeliveryService service(&sessions, &sender, &marker);

    im::push::SendSessionPayloadRequest request;
    request.set_session_id("session-3");
    im::push::SendSessionPayloadResponse response;

    auto status = service.SendSessionPayload(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::PARAM_ERROR);
    EXPECT_TRUE(sender.sent_session_ids.empty());
}

TEST(GatewayPushDeliveryServiceTest, MarkMessageDeliveredDelegates) {
    FakeSessionProvider sessions;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    GatewayPushDeliveryService service(&sessions, &sender, &marker);

    im::push::MarkMessageDeliveredRequest request;
    request.set_msg_id(77);
    request.set_delivered_time(1000);
    im::push::MarkMessageDeliveredResponse response;

    auto status = service.MarkMessageDelivered(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::SUCCESS);
    EXPECT_TRUE(response.marked());
    ASSERT_EQ(marker.marked_msg_ids.size(), 1u);
    EXPECT_EQ(marker.marked_msg_ids[0], 77u);
    EXPECT_EQ(marker.marked_times[0], 1000);
}

TEST(GatewayPushDeliveryServiceTest, DependencyExceptionBecomesBaseError) {
    FakeSessionProvider sessions;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    sender.throw_on_call = true;
    GatewayPushDeliveryService service(&sessions, &sender, &marker);

    im::push::SendSessionPayloadRequest request;
    request.set_session_id("session-4");
    request.set_payload("payload");
    im::push::SendSessionPayloadResponse response;

    auto status = service.SendSessionPayload(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::SERVER_ERROR);
}

} // namespace
