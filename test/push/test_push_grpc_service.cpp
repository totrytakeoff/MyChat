#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <push_grpc_service.hpp>

#include "base.pb.h"
#include "push.pb.h"

namespace {

using im::service::push::PushGrpcService;
using im::service::push::PushNotifier;

class FakePushNotifier : public PushNotifier {
public:
    void notify_user(const std::string& receiver_uid,
                     uint64_t msg_id,
                     const std::string& content,
                     const im::service::push::PushContext& context) override {
        calls.push_back({receiver_uid, msg_id, content, context});
        if (throw_on_notify) {
            throw std::runtime_error("push failed");
        }
    }

    struct Call {
        std::string receiver_uid;
        uint64_t msg_id = 0;
        std::string content;
        im::service::push::PushContext context;
    };

    bool throw_on_notify = false;
    std::vector<Call> calls;
};

TEST(PushGrpcServiceTest, NotifyUserDelegatesToPushNotifier) {
    FakePushNotifier notifier;
    PushGrpcService service(&notifier);
    im::push::NotifyUserRequest request;
    im::push::NotifyUserResponse response;
    request.set_receiver_uid("receiver-1");
    request.set_msg_id(42);
    request.set_content("hello");
    request.set_sender_uid("sender-1");
    request.set_conversation_type("direct");
    request.set_conversation_id("sender-1");

    auto status = service.NotifyUser(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    ASSERT_EQ(notifier.calls.size(), 1u);
    EXPECT_EQ(notifier.calls[0].receiver_uid, "receiver-1");
    EXPECT_EQ(notifier.calls[0].msg_id, 42u);
    EXPECT_EQ(notifier.calls[0].content, "hello");
    EXPECT_EQ(notifier.calls[0].context.sender_uid, "sender-1");
    EXPECT_EQ(notifier.calls[0].context.conversation_type, "direct");
    EXPECT_EQ(notifier.calls[0].context.conversation_id, "sender-1");
    EXPECT_EQ(response.base().error_code(), im::base::SUCCESS);
    EXPECT_TRUE(response.base().error_message().empty());
}

TEST(PushGrpcServiceTest, NotifyUserRejectsMissingRequiredFields) {
    FakePushNotifier notifier;
    PushGrpcService service(&notifier);
    im::push::NotifyUserRequest request;
    im::push::NotifyUserResponse response;

    auto status = service.NotifyUser(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(notifier.calls.empty());
    EXPECT_EQ(response.base().error_code(), im::base::PARAM_ERROR);
}

TEST(PushGrpcServiceTest, NotifyUserReportsMissingNotifier) {
    PushGrpcService service(nullptr);
    im::push::NotifyUserRequest request;
    im::push::NotifyUserResponse response;
    request.set_receiver_uid("receiver-1");
    request.set_msg_id(42);

    auto status = service.NotifyUser(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::SERVER_ERROR);
}

TEST(PushGrpcServiceTest, NotifyUserConvertsNotifierExceptionToBaseError) {
    FakePushNotifier notifier;
    notifier.throw_on_notify = true;
    PushGrpcService service(&notifier);
    im::push::NotifyUserRequest request;
    im::push::NotifyUserResponse response;
    request.set_receiver_uid("receiver-1");
    request.set_msg_id(42);

    auto status = service.NotifyUser(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::SERVER_ERROR);
    EXPECT_EQ(response.base().error_message(), "push failed");
}

} // namespace
