#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <gateway/service_adapters/remote_message_service_adapter.hpp>

#include "base.pb.h"
#include "command.pb.h"
#include "message.pb.h"

namespace {

using im::gateway::MessageRpcClient;
using im::gateway::RemoteMessageServiceAdapter;

class FakeMessageRpcClient final : public MessageRpcClient {
public:
    ::grpc::Status forward_packet(::grpc::ClientContext* /*context*/,
                                  const im::message::MessagePacketRequest& request,
                                  im::message::MessagePacketResponse* response) override {
        requests.push_back(request);
        response->mutable_base()->set_error_code(base_code);
        response->mutable_base()->set_error_message(base_message);
        response->mutable_header()->CopyFrom(request.header());
        response->set_type_name(type_name);
        response->set_payload(payload);
        response->set_http_status(http_status);
        response->mutable_push_event()->set_enabled(push_enabled);
        response->mutable_push_event()->set_receiver_uid(push_receiver);
        response->mutable_push_event()->set_msg_id(push_msg_id);
        response->mutable_push_event()->set_content(push_content);
        response->mutable_push_event()->set_sender_uid(push_sender);
        response->mutable_push_event()->set_conversation_type(push_conversation_type);
        response->mutable_push_event()->set_conversation_id(push_conversation_id);
        return status;
    }

    ::grpc::Status status = ::grpc::Status::OK;
    im::base::ErrorCode base_code = im::base::SUCCESS;
    std::string base_message;
    std::string type_name = "application/json";
    std::string payload = R"({"message":{"msg_id":1001}})";
    int http_status = 201;
    bool push_enabled = true;
    std::string push_receiver = "receiver";
    uint64_t push_msg_id = 1001;
    std::string push_content = "hello";
    std::string push_sender = "sender";
    std::string push_conversation_type = "direct";
    std::string push_conversation_id = "sender";
    std::vector<im::message::MessagePacketRequest> requests;
};

}  // namespace

TEST(RemoteMessageServiceAdapterTest, ForwardMapsPacketBoundary) {
    auto fake = std::make_unique<FakeMessageRpcClient>();
    auto* raw = fake.get();
    RemoteMessageServiceAdapter client(std::move(fake));

    im::message::MessagePacketRequest packet;
    packet.mutable_header()->set_cmd_id(im::command::CMD_SEND_MESSAGE);
    packet.mutable_header()->set_from_uid("sender");
    packet.set_type_name("application/json");
    packet.set_payload(R"({"receiver_uid":"receiver","content":"hello"})");

    const auto result = client.forward(packet);

    ASSERT_EQ(raw->requests.size(), 1u);
    EXPECT_EQ(raw->requests[0].header().from_uid(), "sender");
    EXPECT_EQ(raw->requests[0].type_name(), "application/json");
    EXPECT_EQ(raw->requests[0].payload(), packet.payload());
    EXPECT_EQ(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 201);
    EXPECT_EQ(result.type_name(), "application/json");
    EXPECT_EQ(result.payload(), raw->payload);
    EXPECT_TRUE(result.push_event().enabled());
    EXPECT_EQ(result.push_event().receiver_uid(), "receiver");
    EXPECT_EQ(result.push_event().msg_id(), 1001u);
}

TEST(RemoteMessageServiceAdapterTest, ForwardMapsRpcFailure) {
    auto fake = std::make_unique<FakeMessageRpcClient>();
    fake->status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteMessageServiceAdapter client(std::move(fake));

    im::message::MessagePacketRequest packet;
    packet.mutable_header()->set_cmd_id(im::command::CMD_SEND_MESSAGE);
    packet.set_type_name("application/json");

    const auto result = client.forward(packet);

    EXPECT_EQ(result.base().error_code(), im::base::SERVER_ERROR);
    EXPECT_EQ(result.base().error_message(), "down");
    EXPECT_EQ(result.header().cmd_id(), im::command::CMD_SEND_MESSAGE);
}
