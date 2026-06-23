#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <gateway/service_adapters/remote_user_service_adapter.hpp>

#include "base.pb.h"
#include "command.pb.h"
#include "user.pb.h"

namespace {

using im::gateway::RemoteUserServiceAdapter;
using im::gateway::UserRpcClient;

class FakeUserRpcClient final : public UserRpcClient {
public:
    ::grpc::Status forward_packet(::grpc::ClientContext* /*context*/,
                                  const im::user::UserPacketRequest& request,
                                  im::user::UserPacketResponse* response) override {
        requests.push_back(request);
        response->mutable_base()->set_error_code(base_code);
        response->mutable_base()->set_error_message(base_message);
        response->mutable_header()->CopyFrom(request.header());
        response->set_type_name(type_name);
        response->set_payload(payload);
        response->set_http_status(http_status);
        auto* hint = response->mutable_auth_token_hint();
        hint->set_required(auth_required);
        hint->set_uid(auth_uid);
        hint->set_account(auth_account);
        hint->set_device_id(auth_device_id);
        hint->set_platform(auth_platform);
        return status;
    }

    ::grpc::Status status = ::grpc::Status::OK;
    im::base::ErrorCode base_code = im::base::SUCCESS;
    std::string base_message;
    std::string type_name = "application/json";
    std::string payload = R"({"profile":{"uid":"remote-user-uid"}})";
    int http_status = 200;
    bool auth_required = true;
    std::string auth_uid = "remote-user-uid";
    std::string auth_account = "remote-user-account";
    std::string auth_device_id = "device-web";
    std::string auth_platform = "web";
    std::vector<im::user::UserPacketRequest> requests;
};

}  // namespace

TEST(RemoteUserServiceAdapterTest, ForwardPacketMapsResponseAndAuthHint) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    auto* raw = fake.get();
    RemoteUserServiceAdapter client(std::move(fake));

    im::user::UserPacketRequest packet;
    packet.mutable_header()->set_cmd_id(im::command::CMD_LOGIN);
    packet.mutable_header()->set_device_id("device-web");
    packet.mutable_header()->set_platform("web");
    packet.set_type_name("application/json");
    packet.set_payload(R"({"account":"remote-user-account","password":"pass"})");

    const auto result = client.forward(packet);

    ASSERT_EQ(raw->requests.size(), 1u);
    EXPECT_EQ(raw->requests[0].header().cmd_id(), im::command::CMD_LOGIN);
    EXPECT_EQ(raw->requests[0].header().device_id(), "device-web");
    EXPECT_EQ(raw->requests[0].type_name(), "application/json");
    EXPECT_EQ(raw->requests[0].payload(), packet.payload());
    EXPECT_EQ(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 200);
    EXPECT_EQ(result.type_name(), "application/json");
    EXPECT_EQ(result.payload(), raw->payload);
    EXPECT_TRUE(result.auth_token_hint().required());
    EXPECT_EQ(result.auth_token_hint().uid(), "remote-user-uid");
    EXPECT_EQ(result.auth_token_hint().account(), "remote-user-account");
    EXPECT_EQ(result.auth_token_hint().device_id(), "device-web");
    EXPECT_EQ(result.auth_token_hint().platform(), "web");
}

TEST(RemoteUserServiceAdapterTest, ForwardPacketMapsRpcFailure) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    fake->status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteUserServiceAdapter client(std::move(fake));

    im::user::UserPacketRequest packet;
    packet.mutable_header()->set_cmd_id(im::command::CMD_LOGIN);
    packet.set_type_name("application/json");

    const auto result = client.forward(packet);

    EXPECT_EQ(result.base().error_code(), im::base::SERVER_ERROR);
    EXPECT_EQ(result.base().error_message(), "down");
    EXPECT_EQ(result.header().cmd_id(), im::command::CMD_LOGIN);
}
