#include "remote_message_client.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "base.pb.h"
#include "message.hpp"
#include "../../common/utils/log_manager.hpp"

namespace im::gateway {

namespace {

class GrpcMessageRpcClient final : public MessageRpcClient {
public:
    explicit GrpcMessageRpcClient(const std::string& endpoint)
        : stub_(im::message::MessageService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()))) {}

    ::grpc::Status send_message(::grpc::ClientContext* context,
                                const im::message::SendMessageRequest& request,
                                im::message::SendMessageResponse* response) override {
        return stub_->SendMessage(context, request, response);
    }

    ::grpc::Status get_conversation(
        ::grpc::ClientContext* context,
        const im::message::GetConversationRequest& request,
        im::message::GetConversationResponse* response) override {
        return stub_->GetConversation(context, request, response);
    }

    ::grpc::Status pull_offline(::grpc::ClientContext* context,
                                const im::message::PullOfflineRequest& request,
                                im::message::PullOfflineResponse* response) override {
        return stub_->PullOffline(context, request, response);
    }

    ::grpc::Status mark_delivered(
        ::grpc::ClientContext* context,
        const im::message::MarkMessageDeliveredRequest& request,
        im::message::MarkMessageDeliveredResponse* response) override {
        return stub_->MarkDelivered(context, request, response);
    }

    ::grpc::Status mark_read(::grpc::ClientContext* context,
                             const im::message::MarkMessageReadRequest& request,
                             im::message::MarkMessageReadResponse* response) override {
        return stub_->MarkRead(context, request, response);
    }

private:
    std::unique_ptr<im::message::MessageService::Stub> stub_;
};

im::message::MessageType to_proto_type(im::service::message::MessageType type) {
    switch (type) {
    case im::service::message::MessageType::TEXT:
        return im::message::TEXT;
    case im::service::message::MessageType::IMAGE:
        return im::message::IMAGE;
    case im::service::message::MessageType::FILE:
        return im::message::FILE;
    case im::service::message::MessageType::SYSTEM:
        return im::message::SYSTEM;
    default:
        return im::message::TEXT;
    }
}

im::service::message::MessageType to_service_type(im::message::MessageType type) {
    switch (type) {
    case im::message::TEXT:
        return im::service::message::MessageType::TEXT;
    case im::message::IMAGE:
        return im::service::message::MessageType::IMAGE;
    case im::message::FILE:
        return im::service::message::MessageType::FILE;
    case im::message::SYSTEM:
        return im::service::message::MessageType::SYSTEM;
    default:
        return im::service::message::MessageType::TEXT;
    }
}

im::service::message::MessageData to_data(const im::message::MessageBody& body) {
    im::service::message::MessageData data;
    if (!body.message_id().empty()) {
        try {
            data.msg_id = std::stoull(body.message_id());
        } catch (...) {
            data.msg_id = 0;
        }
    }
    data.sender_uid = body.sender_uid();
    data.receiver_uid = body.receiver_uid();
    data.content = body.content();
    data.msg_type = to_service_type(body.type());
    data.status = body.is_read()
        ? im::service::message::MessageStatus::READ
        : (body.delivered_time() > 0
            ? im::service::message::MessageStatus::DELIVERED
            : im::service::message::MessageStatus::SENT);
    data.create_time = body.create_time();
    data.delivered_time = body.delivered_time();
    data.read_time = body.read_time();
    return data;
}

std::string send_error_code(im::base::ErrorCode code) {
    switch (code) {
    case im::base::PARAM_ERROR:
        return "PARAM_ERROR";
    case im::base::SERVER_ERROR:
        return "REMOTE_MESSAGE_SERVER_ERROR";
    default:
        return "REMOTE_MESSAGE_ERROR";
    }
}

}  // namespace

RemoteMessageClient::RemoteMessageClient(const std::string& endpoint,
                                         std::chrono::milliseconds timeout)
    : RemoteMessageClient(std::make_unique<GrpcMessageRpcClient>(endpoint), timeout) {}

RemoteMessageClient::RemoteMessageClient(std::unique_ptr<MessageRpcClient> client,
                                         std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout) {
    im::utils::LogManager::SetLogToFile("remote_message_client",
                                        "logs/remote_message_client.log");
    logger_ = im::utils::LogManager::GetLogger("remote_message_client");
}

im::service::message::SendResult RemoteMessageClient::send_text_message(
    const im::service::message::SendRequest& request) {
    im::service::message::SendResult result;
    if (!client_) {
        result.error_code = "REMOTE_MESSAGE_UNAVAILABLE";
        result.message = "Remote Message client is not configured";
        return result;
    }

    im::message::SendMessageRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(request.sender_uid);
    rpc_request.mutable_header()->set_to_uid(request.receiver_uid);
    rpc_request.mutable_header()->set_timestamp(static_cast<uint64_t>(request.now_ms));
    rpc_request.mutable_body()->set_type(to_proto_type(request.msg_type));
    rpc_request.mutable_body()->set_content(request.content);

    im::message::SendMessageResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->send_message(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_MESSAGE_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote message send RPC failed {} -> {}: {}",
                      request.sender_uid, request.receiver_uid, status.error_message());
        return result;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = send_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }

    result.ok = true;
    result.data = to_data(rpc_response.message());
    return result;
}

std::vector<im::service::message::MessageData> RemoteMessageClient::get_conversation(
    const std::string& user_a,
    const std::string& user_b,
    int64_t before_time,
    int limit) {
    if (!client_) {
        logger_->warn("Remote conversation lookup skipped: RPC client is not configured");
        return {};
    }

    im::message::GetConversationRequest rpc_request;
    rpc_request.set_user_a(user_a);
    rpc_request.set_user_b(user_b);
    rpc_request.set_before_time(before_time);
    rpc_request.set_limit(limit);

    im::message::GetConversationResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->get_conversation(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote conversation RPC failed for {} <-> {}: {}",
                      user_a, user_b, status.error_message());
        return {};
    }
    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Remote conversation RPC returned business error: {}",
                      rpc_response.base().error_message());
        return {};
    }

    std::vector<im::service::message::MessageData> messages;
    messages.reserve(static_cast<size_t>(rpc_response.messages_size()));
    for (const auto& message : rpc_response.messages()) {
        messages.push_back(to_data(message));
    }
    return messages;
}

std::vector<im::service::message::MessageData> RemoteMessageClient::pull_offline(
    const std::string& receiver_uid,
    int64_t before_time,
    int limit) {
    if (!client_) {
        logger_->warn("Remote offline pull skipped: RPC client is not configured");
        return {};
    }

    im::message::PullOfflineRequest rpc_request;
    rpc_request.set_receiver_uid(receiver_uid);
    rpc_request.set_before_time(before_time);
    rpc_request.set_limit(limit);

    im::message::PullOfflineResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->pull_offline(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote offline pull RPC failed for {}: {}",
                      receiver_uid, status.error_message());
        return {};
    }
    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Remote offline pull RPC returned business error: {}",
                      rpc_response.base().error_message());
        return {};
    }

    std::vector<im::service::message::MessageData> messages;
    messages.reserve(static_cast<size_t>(rpc_response.messages_size()));
    for (const auto& message : rpc_response.messages()) {
        messages.push_back(to_data(message));
    }
    return messages;
}

bool RemoteMessageClient::mark_delivered(uint64_t msg_id, int64_t delivered_time) {
    if (!client_) {
        return false;
    }

    im::message::MarkMessageDeliveredRequest rpc_request;
    rpc_request.set_msg_id(msg_id);
    rpc_request.set_delivered_time(delivered_time);

    im::message::MarkMessageDeliveredResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->mark_delivered(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote mark delivered RPC failed for {}: {}",
                      msg_id, status.error_message());
        return false;
    }
    return rpc_response.base().error_code() == im::base::SUCCESS && rpc_response.marked();
}

bool RemoteMessageClient::mark_read(uint64_t msg_id, int64_t read_time) {
    if (!client_) {
        return false;
    }

    im::message::MarkMessageReadRequest rpc_request;
    rpc_request.set_msg_id(msg_id);
    rpc_request.set_read_time(read_time);

    im::message::MarkMessageReadResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->mark_read(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote mark read RPC failed for {}: {}",
                      msg_id, status.error_message());
        return false;
    }
    return rpc_response.base().error_code() == im::base::SUCCESS && rpc_response.marked();
}

void RemoteMessageClient::apply_deadline(::grpc::ClientContext& context) const {
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }
}

}  // namespace im::gateway
