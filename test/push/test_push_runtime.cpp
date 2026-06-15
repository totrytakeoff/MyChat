#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <push_runtime.hpp>

#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/proto/command.pb.h"
#include "../../common/proto/push.pb.h"

namespace {

using json = nlohmann::json;
using im::network::ProtobufCodec;
using im::service::push::PlatformFilterFanoutPolicy;
using im::service::push::PushContext;
using im::service::push::PushDeliveryMarker;
using im::service::push::PushPayloadSender;
using im::service::push::PushRuntime;
using im::service::push::PushSessionInfo;
using im::service::push::PushSessionProvider;

class FakeSessionProvider : public PushSessionProvider {
public:
    std::vector<PushSessionInfo> get_sessions(const std::string& receiver_uid) override {
        requested_uid = receiver_uid;
        return sessions;
    }

    std::string requested_uid;
    std::vector<PushSessionInfo> sessions;
};

class FakePayloadSender : public PushPayloadSender {
public:
    bool send_payload(const std::string& session_id,
                      const std::string& payload) override {
        sent_sessions.push_back(session_id);
        sent_payloads.push_back(payload);
        return send_success;
    }

    bool send_success = true;
    std::vector<std::string> sent_sessions;
    std::vector<std::string> sent_payloads;
};

class FakeDeliveryMarker : public PushDeliveryMarker {
public:
    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override {
        marked = true;
        marked_msg_id = msg_id;
        marked_time = delivered_time;
        return true;
    }

    bool marked = false;
    uint64_t marked_msg_id = 0;
    int64_t marked_time = 0;
};

PushSessionInfo make_session(const std::string& session_id,
                             const std::string& platform,
                             std::chrono::system_clock::time_point connect_time) {
    return {
        .session_id = session_id,
        .platform = platform,
        .connect_time = connect_time,
    };
}

TEST(PushRuntimeTest, NoSessionsDoesNotSendOrMarkDelivered) {
    FakeSessionProvider provider;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    PushRuntime runtime(&provider, &sender, &marker);

    runtime.notify_user("receiver-1", 42, "hello");

    EXPECT_EQ(provider.requested_uid, "receiver-1");
    EXPECT_TRUE(sender.sent_sessions.empty());
    EXPECT_FALSE(marker.marked);
}

TEST(PushRuntimeTest, SendsSelectedSessionsAndMarksDeliveredOnSuccess) {
    FakeSessionProvider provider;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    PushRuntime runtime(&provider, &sender, &marker);
    auto now = std::chrono::system_clock::now();
    provider.sessions = {
        make_session("web-1", "web", now),
        make_session("mobile-1", "mobile", now),
    };
    runtime.set_fanout_policy(std::make_unique<PlatformFilterFanoutPolicy>(
        std::vector<std::string>{"mobile"}));

    runtime.notify_user("receiver-2", 1001, "content");

    ASSERT_EQ(sender.sent_sessions.size(), 1u);
    EXPECT_EQ(sender.sent_sessions[0], "mobile-1");
    ASSERT_EQ(sender.sent_payloads.size(), 1u);
    EXPECT_FALSE(sender.sent_payloads[0].empty());
    EXPECT_TRUE(marker.marked);
    EXPECT_EQ(marker.marked_msg_id, 1001u);
    EXPECT_GT(marker.marked_time, 0);
}

TEST(PushRuntimeTest, EncodesConversationContextInPayload) {
    FakeSessionProvider provider;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    PushRuntime runtime(&provider, &sender, &marker);
    provider.sessions = {
        make_session("web-1", "web", std::chrono::system_clock::now()),
    };

    PushContext context;
    context.sender_uid = "sender-1";
    context.conversation_type = "direct";
    context.conversation_id = "sender-1";

    runtime.notify_user("receiver-1", 1004, "hello push", context);

    ASSERT_EQ(sender.sent_payloads.size(), 1u);

    im::base::IMHeader header;
    im::push::PushRequest request;
    ASSERT_TRUE(ProtobufCodec::decode(sender.sent_payloads[0], header, request));

    EXPECT_EQ(header.cmd_id(), im::command::CMD_PUSH_MESSAGE);
    EXPECT_EQ(header.from_uid(), "sender-1");
    EXPECT_EQ(header.to_uid(), "receiver-1");
    EXPECT_EQ(request.body().type(), im::push::PUSH_MESSAGE);
    EXPECT_EQ(request.body().content(), "hello push");
    EXPECT_EQ(request.body().related_message_id(), "1004");

    ASSERT_FALSE(request.body().ext().empty());
    const auto ext = json::parse(request.body().ext());
    EXPECT_EQ(ext.at("sender_uid").get<std::string>(), "sender-1");
    EXPECT_EQ(ext.at("conversation_type").get<std::string>(), "direct");
    EXPECT_EQ(ext.at("conversation_id").get<std::string>(), "sender-1");
}

TEST(PushRuntimeTest, FailedSendsDoNotMarkDelivered) {
    FakeSessionProvider provider;
    FakePayloadSender sender;
    FakeDeliveryMarker marker;
    PushRuntime runtime(&provider, &sender, &marker);
    provider.sessions = {
        make_session("sess-1", "web", std::chrono::system_clock::now()),
    };
    sender.send_success = false;

    runtime.notify_user("receiver-3", 1002, "content");

    ASSERT_EQ(sender.sent_sessions.size(), 1u);
    EXPECT_EQ(sender.sent_sessions[0], "sess-1");
    EXPECT_FALSE(marker.marked);
}

TEST(PushRuntimeTest, NullDependenciesReturnSilently) {
    PushRuntime runtime(nullptr, nullptr, nullptr);

    EXPECT_NO_THROW(runtime.notify_user("receiver-4", 1003, "content"));
}

} // anonymous namespace
