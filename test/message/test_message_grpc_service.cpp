#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <message.hpp>
#include <message_grpc_service.hpp>
#include <message_service.hpp>

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1718300000000;
constexpr int64_t kLaterMs = 1718300005000;

class MessageGrpcServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        Cleanup();
        core_ = std::make_unique<im::service::message::MessageService>(db_);
        grpc_ = std::make_unique<im::service::message::MessageGrpcService>(core_.get());
    }

    void TearDown() override {
        grpc_.reset();
        core_.reset();
        Cleanup();
    }

    static void EnsureTable() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void Cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_messages"
            WHERE "sender_uid" LIKE 'grpc-message-test-%'
               OR "receiver_uid" LIKE 'grpc-message-test-%')");
        t.commit();
    }

    im::message::SendMessageResponse SendText(const std::string& sender,
                                              const std::string& receiver,
                                              const std::string& content,
                                              int64_t timestamp) {
        im::message::SendMessageRequest request;
        request.mutable_header()->set_from_uid(sender);
        request.mutable_header()->set_to_uid(receiver);
        request.mutable_header()->set_timestamp(static_cast<uint64_t>(timestamp));
        request.mutable_body()->set_type(im::message::TEXT);
        request.mutable_body()->set_content(content);

        im::message::SendMessageResponse response;
        grpc_->SendMessage(nullptr, &request, &response);
        return response;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<im::service::message::MessageService> core_;
    std::unique_ptr<im::service::message::MessageGrpcService> grpc_;
};

}  // namespace

TEST_F(MessageGrpcServiceTest, SendMessagePersistsAndReturnsStoredBody) {
    auto response = SendText("grpc-message-test-send-a",
                             "grpc-message-test-send-b",
                             "hello over grpc",
                             kNowMs);

    ASSERT_EQ(response.base().error_code(), im::base::SUCCESS);
    EXPECT_FALSE(response.message().message_id().empty());
    EXPECT_EQ(response.message().sender_uid(), "grpc-message-test-send-a");
    EXPECT_EQ(response.message().receiver_uid(), "grpc-message-test-send-b");
    EXPECT_EQ(response.message().content(), "hello over grpc");
    EXPECT_EQ(response.message().type(), im::message::TEXT);
    EXPECT_EQ(response.message().create_time(), kNowMs);
}

TEST_F(MessageGrpcServiceTest, SendMessageRejectsMissingReceiver) {
    im::message::SendMessageRequest request;
    request.mutable_header()->set_from_uid("grpc-message-test-param-a");
    request.mutable_body()->set_content("missing receiver");

    im::message::SendMessageResponse response;
    grpc_->SendMessage(nullptr, &request, &response);

    EXPECT_EQ(response.base().error_code(), im::base::PARAM_ERROR);
}

TEST_F(MessageGrpcServiceTest, GetConversationReturnsPersistedMessages) {
    auto first = SendText("grpc-message-test-cvs-a",
                          "grpc-message-test-cvs-b",
                          "first",
                          kNowMs);
    auto second = SendText("grpc-message-test-cvs-b",
                           "grpc-message-test-cvs-a",
                           "second",
                           kNowMs + 1000);
    ASSERT_EQ(first.base().error_code(), im::base::SUCCESS);
    ASSERT_EQ(second.base().error_code(), im::base::SUCCESS);

    im::message::GetConversationRequest request;
    request.set_user_a("grpc-message-test-cvs-a");
    request.set_user_b("grpc-message-test-cvs-b");
    request.set_before_time(kNowMs + 5000);
    request.set_limit(10);

    im::message::GetConversationResponse response;
    grpc_->GetConversation(nullptr, &request, &response);

    ASSERT_EQ(response.base().error_code(), im::base::SUCCESS);
    ASSERT_GE(response.messages_size(), 2);
    EXPECT_EQ(response.messages(0).content(), "first");
    EXPECT_EQ(response.messages(1).content(), "second");
}

TEST_F(MessageGrpcServiceTest, PullOfflineAndMarkDelivered) {
    auto sent = SendText("grpc-message-test-offline-a",
                         "grpc-message-test-offline-b",
                         "offline message",
                         kNowMs);
    ASSERT_EQ(sent.base().error_code(), im::base::SUCCESS);
    const uint64_t msg_id = std::stoull(sent.message().message_id());

    im::message::PullOfflineRequest pull_request;
    pull_request.set_receiver_uid("grpc-message-test-offline-b");
    pull_request.set_before_time(kNowMs + 5000);
    pull_request.set_limit(10);

    im::message::PullOfflineResponse pull_response;
    grpc_->PullOffline(nullptr, &pull_request, &pull_response);

    ASSERT_EQ(pull_response.base().error_code(), im::base::SUCCESS);
    ASSERT_GE(pull_response.messages_size(), 1);
    EXPECT_EQ(pull_response.messages(0).message_id(), sent.message().message_id());

    im::message::MarkMessageDeliveredRequest mark_request;
    mark_request.set_msg_id(msg_id);
    mark_request.set_delivered_time(kLaterMs);

    im::message::MarkMessageDeliveredResponse mark_response;
    grpc_->MarkDelivered(nullptr, &mark_request, &mark_response);

    EXPECT_EQ(mark_response.base().error_code(), im::base::SUCCESS);
    EXPECT_TRUE(mark_response.marked());

    im::message::PullOfflineResponse after_mark_response;
    grpc_->PullOffline(nullptr, &pull_request, &after_mark_response);
    EXPECT_EQ(after_mark_response.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(after_mark_response.messages_size(), 0);
}

TEST_F(MessageGrpcServiceTest, MarkReadReturnsNotFoundForUnknownMessage) {
    im::message::MarkMessageReadRequest request;
    request.set_msg_id(99999999);
    request.set_read_time(kLaterMs);

    im::message::MarkMessageReadResponse response;
    grpc_->MarkRead(nullptr, &request, &response);

    EXPECT_EQ(response.base().error_code(), im::base::NOT_FOUND);
    EXPECT_FALSE(response.marked());
}
