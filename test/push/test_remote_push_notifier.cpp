#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gateway/push/remote_push_notifier.hpp>

#include "base.pb.h"
#include "push.pb.h"

namespace {

using im::gateway::PushRpcClient;
using im::gateway::RemotePushNotifier;

class FakePushRpcClient : public PushRpcClient {
public:
    ::grpc::Status notify_user(::grpc::ClientContext* /*context*/,
                               const im::push::NotifyUserRequest& request,
                               im::push::NotifyUserResponse* response) override {
        requests.push_back(request);
        response->mutable_base()->set_error_code(base_error_code);
        response->mutable_base()->set_error_message(base_error_message);
        return rpc_status;
    }

    ::grpc::Status rpc_status = ::grpc::Status::OK;
    im::base::ErrorCode base_error_code = im::base::SUCCESS;
    std::string base_error_message;
    std::vector<im::push::NotifyUserRequest> requests;
};

TEST(RemotePushNotifierTest, NotifyUserSendsExpectedRpcRequest) {
    auto fake = std::make_unique<FakePushRpcClient>();
    auto* raw = fake.get();
    RemotePushNotifier notifier(std::move(fake));

    im::service::push::PushContext context;
    context.sender_uid = "sender-1";
    context.conversation_type = "direct";
    context.conversation_id = "sender-1";
    notifier.notify_user("receiver-1", 42, "hello", context);

    ASSERT_EQ(raw->requests.size(), 1u);
    EXPECT_EQ(raw->requests[0].receiver_uid(), "receiver-1");
    EXPECT_EQ(raw->requests[0].msg_id(), 42u);
    EXPECT_EQ(raw->requests[0].content(), "hello");
    EXPECT_EQ(raw->requests[0].sender_uid(), "sender-1");
    EXPECT_EQ(raw->requests[0].conversation_type(), "direct");
    EXPECT_EQ(raw->requests[0].conversation_id(), "sender-1");
}

TEST(RemotePushNotifierTest, RpcFailureDoesNotThrow) {
    auto fake = std::make_unique<FakePushRpcClient>();
    fake->rpc_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    auto* raw = fake.get();
    RemotePushNotifier notifier(std::move(fake));

    EXPECT_NO_THROW(notifier.notify_user("receiver-2", 43, "content"));
    ASSERT_EQ(raw->requests.size(), 1u);
    EXPECT_EQ(raw->requests[0].receiver_uid(), "receiver-2");
}

TEST(RemotePushNotifierTest, BaseErrorDoesNotThrow) {
    auto fake = std::make_unique<FakePushRpcClient>();
    fake->base_error_code = im::base::SERVER_ERROR;
    fake->base_error_message = "server failed";
    auto* raw = fake.get();
    RemotePushNotifier notifier(std::move(fake));

    EXPECT_NO_THROW(notifier.notify_user("receiver-3", 44, "content"));
    ASSERT_EQ(raw->requests.size(), 1u);
    EXPECT_EQ(raw->requests[0].msg_id(), 44u);
}

TEST(RemotePushNotifierTest, MissingClientDoesNotThrow) {
    RemotePushNotifier notifier(std::unique_ptr<PushRpcClient>{});

    EXPECT_NO_THROW(notifier.notify_user("receiver-4", 45, "content"));
}

} // namespace
