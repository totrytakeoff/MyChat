#ifndef GATEWAY_HTTP_REMOTE_MESSAGE_CLIENT_HPP
#define GATEWAY_HTTP_REMOTE_MESSAGE_CLIENT_HPP

#include <chrono>
#include <memory>
#include <string>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "message.grpc.pb.h"
#include "message_client.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class MessageRpcClient {
public:
    virtual ~MessageRpcClient() = default;

    virtual ::grpc::Status send_message(
        ::grpc::ClientContext* context,
        const im::message::SendMessageRequest& request,
        im::message::SendMessageResponse* response) = 0;

    virtual ::grpc::Status get_conversation(
        ::grpc::ClientContext* context,
        const im::message::GetConversationRequest& request,
        im::message::GetConversationResponse* response) = 0;

    virtual ::grpc::Status pull_offline(
        ::grpc::ClientContext* context,
        const im::message::PullOfflineRequest& request,
        im::message::PullOfflineResponse* response) = 0;

    virtual ::grpc::Status mark_delivered(
        ::grpc::ClientContext* context,
        const im::message::MarkMessageDeliveredRequest& request,
        im::message::MarkMessageDeliveredResponse* response) = 0;

    virtual ::grpc::Status mark_read(
        ::grpc::ClientContext* context,
        const im::message::MarkMessageReadRequest& request,
        im::message::MarkMessageReadResponse* response) = 0;
};

class RemoteMessageClient final : public MessageClient {
public:
    explicit RemoteMessageClient(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteMessageClient(
        std::unique_ptr<MessageRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    im::service::message::SendResult send_text_message(
        const im::service::message::SendRequest& request) override;

    std::vector<im::service::message::MessageData> get_conversation(
        const std::string& user_a,
        const std::string& user_b,
        int64_t before_time,
        int limit) override;

    std::vector<im::service::message::MessageData> pull_offline(
        const std::string& receiver_uid,
        int64_t before_time,
        int limit) override;

    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override;
    bool mark_read(uint64_t msg_id, int64_t read_time) override;

private:
    void apply_deadline(::grpc::ClientContext& context) const;

    std::unique_ptr<MessageRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace im::gateway

#endif  // GATEWAY_HTTP_REMOTE_MESSAGE_CLIENT_HPP
