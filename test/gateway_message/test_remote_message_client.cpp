#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <gateway/http/remote_message_client.hpp>
#include <message.hpp>

#include "base.pb.h"
#include "message.pb.h"

namespace {

using im::gateway::MessageRpcClient;
using im::gateway::RemoteMessageClient;

class FakeMessageRpcClient final : public MessageRpcClient {
public:
    ::grpc::Status send_message(::grpc::ClientContext* /*context*/,
                                const im::message::SendMessageRequest& request,
                                im::message::SendMessageResponse* response) override {
        send_requests.push_back(request);
        response->mutable_base()->set_error_code(send_base_code);
        response->mutable_base()->set_error_message(send_base_message);
        if (send_base_code == im::base::SUCCESS) {
            fill_body(response->mutable_message(),
                      "1001",
                      request.header().from_uid(),
                      request.header().to_uid(),
                      request.body().content());
        }
        return send_status;
    }

    ::grpc::Status get_conversation(::grpc::ClientContext* /*context*/,
                                    const im::message::GetConversationRequest& request,
                                    im::message::GetConversationResponse* response) override {
        conversation_requests.push_back(request);
        response->mutable_base()->set_error_code(conversation_base_code);
        if (conversation_base_code == im::base::SUCCESS) {
            fill_body(response->add_messages(), "1002", request.user_a(), request.user_b(), "history");
        }
        return conversation_status;
    }

    ::grpc::Status pull_offline(::grpc::ClientContext* /*context*/,
                                const im::message::PullOfflineRequest& request,
                                im::message::PullOfflineResponse* response) override {
        offline_requests.push_back(request);
        response->mutable_base()->set_error_code(offline_base_code);
        if (offline_base_code == im::base::SUCCESS) {
            fill_body(response->add_messages(), "1003", "sender", request.receiver_uid(), "offline");
        }
        return offline_status;
    }

    ::grpc::Status mark_delivered(::grpc::ClientContext* /*context*/,
                                  const im::message::MarkMessageDeliveredRequest& request,
                                  im::message::MarkMessageDeliveredResponse* response) override {
        delivered_requests.push_back(request);
        response->mutable_base()->set_error_code(delivered_base_code);
        response->set_marked(delivered_marked);
        return delivered_status;
    }

    ::grpc::Status mark_read(::grpc::ClientContext* /*context*/,
                             const im::message::MarkMessageReadRequest& request,
                             im::message::MarkMessageReadResponse* response) override {
        read_requests.push_back(request);
        response->mutable_base()->set_error_code(read_base_code);
        response->set_marked(read_marked);
        return read_status;
    }

    static void fill_body(im::message::MessageBody* body,
                          const std::string& id,
                          const std::string& sender,
                          const std::string& receiver,
                          const std::string& content) {
        body->set_message_id(id);
        body->set_sender_uid(sender);
        body->set_receiver_uid(receiver);
        body->set_content(content);
        body->set_type(im::message::TEXT);
        body->set_create_time(1234);
    }

    ::grpc::Status send_status = ::grpc::Status::OK;
    ::grpc::Status conversation_status = ::grpc::Status::OK;
    ::grpc::Status offline_status = ::grpc::Status::OK;
    ::grpc::Status delivered_status = ::grpc::Status::OK;
    ::grpc::Status read_status = ::grpc::Status::OK;
    im::base::ErrorCode send_base_code = im::base::SUCCESS;
    im::base::ErrorCode conversation_base_code = im::base::SUCCESS;
    im::base::ErrorCode offline_base_code = im::base::SUCCESS;
    im::base::ErrorCode delivered_base_code = im::base::SUCCESS;
    im::base::ErrorCode read_base_code = im::base::SUCCESS;
    std::string send_base_message;
    bool delivered_marked = true;
    bool read_marked = true;
    std::vector<im::message::SendMessageRequest> send_requests;
    std::vector<im::message::GetConversationRequest> conversation_requests;
    std::vector<im::message::PullOfflineRequest> offline_requests;
    std::vector<im::message::MarkMessageDeliveredRequest> delivered_requests;
    std::vector<im::message::MarkMessageReadRequest> read_requests;
};

}  // namespace

TEST(RemoteMessageClientTest, SendMapsSuccess) {
    auto fake = std::make_unique<FakeMessageRpcClient>();
    auto* raw = fake.get();
    RemoteMessageClient client(std::move(fake));

    im::service::message::SendRequest request;
    request.sender_uid = "remote-message-sender";
    request.receiver_uid = "remote-message-receiver";
    request.content = "hello";
    request.msg_type = im::service::message::MessageType::TEXT;
    request.now_ms = 1234;

    auto result = client.send_text_message(request);

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(raw->send_requests.size(), 1u);
    EXPECT_EQ(raw->send_requests[0].header().from_uid(), request.sender_uid);
    EXPECT_EQ(raw->send_requests[0].header().to_uid(), request.receiver_uid);
    EXPECT_EQ(raw->send_requests[0].body().content(), request.content);
    EXPECT_EQ(result.data.msg_id, 1001u);
    EXPECT_EQ(result.data.sender_uid, request.sender_uid);
}

TEST(RemoteMessageClientTest, SendMapsParamErrorAndRpcFailure) {
    auto fake = std::make_unique<FakeMessageRpcClient>();
    fake->send_base_code = im::base::PARAM_ERROR;
    fake->send_base_message = "bad request";
    RemoteMessageClient param_client(std::move(fake));

    auto param_result = param_client.send_text_message({});
    EXPECT_FALSE(param_result.ok);
    EXPECT_EQ(param_result.error_code, "PARAM_ERROR");
    EXPECT_EQ(param_result.message, "bad request");

    auto failing = std::make_unique<FakeMessageRpcClient>();
    failing->send_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteMessageClient failing_client(std::move(failing));

    auto rpc_result = failing_client.send_text_message({});
    EXPECT_FALSE(rpc_result.ok);
    EXPECT_EQ(rpc_result.error_code, "REMOTE_MESSAGE_UNAVAILABLE");
}

TEST(RemoteMessageClientTest, QueryAndMarkMapResults) {
    auto fake = std::make_unique<FakeMessageRpcClient>();
    auto* raw = fake.get();
    RemoteMessageClient client(std::move(fake));

    auto history = client.get_conversation("a", "b", 9999, 10);
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].content, "history");
    ASSERT_EQ(raw->conversation_requests.size(), 1u);
    EXPECT_EQ(raw->conversation_requests[0].user_a(), "a");

    auto offline = client.pull_offline("receiver", 8888, 5);
    ASSERT_EQ(offline.size(), 1u);
    EXPECT_EQ(offline[0].receiver_uid, "receiver");
    ASSERT_EQ(raw->offline_requests.size(), 1u);
    EXPECT_EQ(raw->offline_requests[0].receiver_uid(), "receiver");

    EXPECT_TRUE(client.mark_delivered(100, 200));
    EXPECT_TRUE(client.mark_read(100, 300));
    ASSERT_EQ(raw->delivered_requests.size(), 1u);
    ASSERT_EQ(raw->read_requests.size(), 1u);
    EXPECT_EQ(raw->delivered_requests[0].msg_id(), 100u);
    EXPECT_EQ(raw->read_requests[0].read_time(), 300);
}

TEST(RemoteMessageClientTest, QueryAndMarkFailuresReturnConservativeValues) {
    auto failing = std::make_unique<FakeMessageRpcClient>();
    failing->conversation_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    failing->offline_base_code = im::base::SERVER_ERROR;
    failing->delivered_marked = false;
    failing->read_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteMessageClient client(std::move(failing));

    EXPECT_TRUE(client.get_conversation("a", "b", 0, 10).empty());
    EXPECT_TRUE(client.pull_offline("receiver", 0, 10).empty());
    EXPECT_FALSE(client.mark_delivered(1, 2));
    EXPECT_FALSE(client.mark_read(1, 2));
}
