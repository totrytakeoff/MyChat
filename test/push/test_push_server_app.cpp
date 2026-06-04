#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <push_server_app.hpp>

#include "base.pb.h"
#include "push.grpc.pb.h"

namespace {

TEST(PushServerAppTest, NotifyUserRpcReturnsSuccess) {
    im::service::push::PushServerConfig config;
    config.listen_address = "127.0.0.1:0";

    im::service::push::PushServerApp server(config);
    ASSERT_TRUE(server.start());
    ASSERT_GT(server.selected_port(), 0);

    const std::string endpoint = "127.0.0.1:" + std::to_string(server.selected_port());
    auto channel = ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials());
    auto stub = im::push::PushService::NewStub(channel);

    im::push::NotifyUserRequest request;
    request.set_receiver_uid("receiver-1");
    request.set_msg_id(1001);
    request.set_content("hello");

    im::push::NotifyUserResponse response;
    ::grpc::ClientContext context;
    auto status = stub->NotifyUser(&context, request, &response);

    EXPECT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(response.base().error_code(), im::base::SUCCESS);

    server.shutdown();
}

TEST(PushServerAppTest, StartReturnsFalseForInvalidListenAddress) {
    im::service::push::PushServerConfig config;
    config.listen_address = "127.0.0.1:not-a-port";

    im::service::push::PushServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}

} // namespace
