#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <gateway/service_adapters/remote_friend_service_adapter.hpp>

#include "base.pb.h"
#include "friend.pb.h"

namespace {

using im::gateway::FriendRpcClient;
using im::gateway::RemoteFriendServiceAdapter;

class FakeFriendRpcClient final : public FriendRpcClient {
public:
    ::grpc::Status forward_packet(::grpc::ClientContext* /*context*/,
                                  const im::friend_::FriendPacketRequest& request,
                                  im::friend_::FriendPacketResponse* response) override {
        requests.push_back(request);
        response->mutable_base()->set_error_code(base_code);
        response->mutable_base()->set_error_message(base_message);
        response->mutable_header()->CopyFrom(request.header());
        response->set_type_name(type_name);
        response->set_payload(payload);
        response->set_http_status(http_status);
        return status;
    }

    ::grpc::Status status = ::grpc::Status::OK;
    im::base::ErrorCode base_code = im::base::SUCCESS;
    std::string base_message;
    int http_status = 200;
    std::string type_name = "application/json";
    std::string payload = R"({"ok":true})";
    std::vector<im::friend_::FriendPacketRequest> requests;
};

}  // namespace

TEST(RemoteFriendServiceAdapterTest, ForwardPacketMapsResponse) {
    auto fake = std::make_unique<FakeFriendRpcClient>();
    auto* raw = fake.get();
    RemoteFriendServiceAdapter client(std::move(fake));

    im::friend_::FriendPacketRequest packet;
    packet.mutable_header()->set_cmd_id(3001);
    packet.mutable_header()->set_from_uid("sender");
    packet.set_type_name("application/json");
    packet.set_payload(R"({"target_uid":"target"})");

    const auto result = client.forward(packet);

    ASSERT_EQ(raw->requests.size(), 1u);
    EXPECT_EQ(raw->requests[0].header().from_uid(), "sender");
    EXPECT_EQ(raw->requests[0].type_name(), "application/json");
    EXPECT_EQ(raw->requests[0].payload(), packet.payload());
    EXPECT_EQ(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 200);
    EXPECT_EQ(result.type_name(), "application/json");
    EXPECT_EQ(result.payload(), R"({"ok":true})");
}

TEST(RemoteFriendServiceAdapterTest, ForwardPacketMapsRpcFailure) {
    auto fake = std::make_unique<FakeFriendRpcClient>();
    fake->status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteFriendServiceAdapter client(std::move(fake));

    im::friend_::FriendPacketRequest packet;
    packet.mutable_header()->set_cmd_id(3001);

    const auto result = client.forward(packet);

    EXPECT_EQ(result.base().error_code(), im::base::SERVER_ERROR);
    EXPECT_EQ(result.base().error_message(), "down");
    EXPECT_EQ(result.header().cmd_id(), 3001u);
}
