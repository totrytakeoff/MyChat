#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <gateway/service_adapters/remote_group_service_adapter.hpp>

#include "base.pb.h"
#include "command.pb.h"
#include "group.pb.h"

namespace {

using im::gateway::GroupRpcClient;
using im::gateway::RemoteGroupServiceAdapter;

class FakeGroupRpcClient final : public GroupRpcClient {
public:
    ::grpc::Status forward_packet(::grpc::ClientContext* /*context*/,
                                  const im::group::GroupPacketRequest& request,
                                  im::group::GroupPacketResponse* response) override {
        requests.push_back(request);
        response->mutable_base()->set_error_code(base_code);
        response->mutable_base()->set_error_message(base_message);
        response->mutable_header()->CopyFrom(request.header());
        response->set_type_name(type_name);
        response->set_payload(payload);
        response->set_http_status(http_status);
        if (push_enabled) {
            auto* event = response->add_push_events();
            event->set_enabled(true);
            event->set_receiver_uid("receiver-uid");
            event->set_msg_id(99);
            event->set_content("group packet push");
            event->set_sender_uid(request.header().from_uid());
            event->set_conversation_type("group");
            event->set_conversation_id("42");
        }
        return status;
    }

    ::grpc::Status status = ::grpc::Status::OK;
    im::base::ErrorCode base_code = im::base::SUCCESS;
    std::string base_message;
    int http_status = 201;
    std::string type_name = "application/json";
    std::string payload = R"({"msg_id":99})";
    bool push_enabled = true;
    std::vector<im::group::GroupPacketRequest> requests;
};

}  // namespace

TEST(RemoteGroupServiceAdapterTest, ForwardPacketMapsResponseAndPushEvents) {
    auto fake = std::make_unique<FakeGroupRpcClient>();
    auto* raw = fake.get();
    RemoteGroupServiceAdapter client(std::move(fake));

    im::group::GroupPacketRequest packet;
    packet.mutable_header()->set_cmd_id(im::command::CMD_SEND_GROUP_MESSAGE);
    packet.mutable_header()->set_from_uid("sender-uid");
    packet.set_type_name("application/json");
    packet.set_payload(R"({"group_id":42,"content":"hello"})");

    const auto result = client.forward(packet);

    ASSERT_EQ(raw->requests.size(), 1u);
    EXPECT_EQ(raw->requests[0].header().cmd_id(), im::command::CMD_SEND_GROUP_MESSAGE);
    EXPECT_EQ(raw->requests[0].header().from_uid(), "sender-uid");
    EXPECT_EQ(raw->requests[0].type_name(), "application/json");
    EXPECT_EQ(raw->requests[0].payload(), packet.payload());
    EXPECT_EQ(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 201);
    EXPECT_EQ(result.payload(), R"({"msg_id":99})");
    ASSERT_EQ(result.push_events_size(), 1);
    EXPECT_TRUE(result.push_events(0).enabled());
    EXPECT_EQ(result.push_events(0).receiver_uid(), "receiver-uid");
    EXPECT_EQ(result.push_events(0).msg_id(), 99u);
    EXPECT_EQ(result.push_events(0).sender_uid(), "sender-uid");
    EXPECT_EQ(result.push_events(0).conversation_type(), "group");
    EXPECT_EQ(result.push_events(0).conversation_id(), "42");
}

TEST(RemoteGroupServiceAdapterTest, ForwardPacketMapsRpcFailure) {
    auto fake = std::make_unique<FakeGroupRpcClient>();
    fake->status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteGroupServiceAdapter client(std::move(fake));

    im::group::GroupPacketRequest packet;
    packet.mutable_header()->set_cmd_id(im::command::CMD_GET_GROUP_LIST);
    packet.mutable_header()->set_from_uid("owner");
    packet.set_type_name("application/json");

    const auto result = client.forward(packet);

    EXPECT_EQ(result.base().error_code(), im::base::SERVER_ERROR);
    EXPECT_EQ(result.base().error_message(), "down");
    EXPECT_EQ(result.header().cmd_id(), im::command::CMD_GET_GROUP_LIST);
}
