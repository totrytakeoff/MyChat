#include "message_grpc_service.hpp"

#include "message_packet_dispatcher.hpp"
#include "message_service.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

#include <grpcpp/server_context.h>

#include "base.pb.h"
#include "message.hpp"
#include "message.pb.h"

namespace im::service::message {

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void set_base(im::base::BaseResponse* base,
              im::base::ErrorCode code,
              const std::string& message = {}) {
    base->set_error_code(code);
    base->set_error_message(message);
}

im::base::ErrorCode send_error_code(const std::string& error_code) {
    if (error_code == "EMPTY_SENDER" ||
        error_code == "EMPTY_RECEIVER" ||
        error_code == "EMPTY_CONTENT") {
        return im::base::PARAM_ERROR;
    }
    return im::base::SERVER_ERROR;
}

MessageType to_service_type(im::message::MessageType type) {
    switch (type) {
    case im::message::TEXT:
        return MessageType::TEXT;
    case im::message::IMAGE:
        return MessageType::IMAGE;
    case im::message::FILE:
        return MessageType::FILE;
    case im::message::SYSTEM:
        return MessageType::SYSTEM;
    case im::message::AUDIO:
    case im::message::VIDEO:
    case im::message::RECALL:
    case im::message::CARD:
    case im::message::LOCATION:
    case im::message::FORWARD:
    case im::message::CUSTOM:
    default:
        return MessageType::TEXT;
    }
}

im::message::MessageType to_proto_type(MessageType type) {
    switch (type) {
    case MessageType::TEXT:
        return im::message::TEXT;
    case MessageType::IMAGE:
        return im::message::IMAGE;
    case MessageType::FILE:
        return im::message::FILE;
    case MessageType::SYSTEM:
        return im::message::SYSTEM;
    default:
        return im::message::TEXT;
    }
}

void fill_message_body(const MessageData& data, im::message::MessageBody* body) {
    body->set_message_id(std::to_string(data.msg_id));
    body->set_type(to_proto_type(data.msg_type));
    body->set_content(data.content);
    body->set_is_read(data.read_time > 0 || data.status == MessageStatus::READ);
    body->set_sender_uid(data.sender_uid);
    body->set_receiver_uid(data.receiver_uid);
    body->set_create_time(data.create_time);
    body->set_delivered_time(data.delivered_time);
    body->set_read_time(data.read_time);
}

template <typename Response>
bool require_service(MessageService* service, Response* response) {
    if (!response) {
        return false;
    }
    if (!service) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR,
                 "MessageService is not available");
        return false;
    }
    return true;
}

}  // namespace

MessageGrpcService::MessageGrpcService(MessageService* message_service)
    : message_service_(message_service) {}

::grpc::Status MessageGrpcService::ForwardPacket(
    ::grpc::ServerContext* /*context*/,
    const im::message::MessagePacketRequest* request,
    im::message::MessagePacketResponse* response) {
    if (!require_service(message_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "im::message::MessagePacketRequest request is null");
        return ::grpc::Status::OK;
    }

    try {
        MessagePacketDispatcher dispatcher(message_service_);
        response->CopyFrom(dispatcher.handle(*request));
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status MessageGrpcService::SendMessage(
    ::grpc::ServerContext* /*context*/,
    const im::message::SendMessageRequest* request,
    im::message::SendMessageResponse* response) {
    if (!require_service(message_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "SendMessage request is null");
        return ::grpc::Status::OK;
    }

    try {
        SendRequest service_request;
        service_request.sender_uid = request->header().from_uid();
        service_request.receiver_uid = request->header().to_uid();
        service_request.content = request->body().content();
        service_request.msg_type = to_service_type(request->body().type());
        service_request.now_ms = static_cast<int64_t>(request->header().timestamp());
        if (service_request.now_ms <= 0) {
            service_request.now_ms = now_ms();
        }

        const auto result = message_service_->send_text_message(service_request);
        if (!result.ok) {
            set_base(response->mutable_base(), send_error_code(result.error_code), result.message);
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        fill_message_body(result.data, response->mutable_message());
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status MessageGrpcService::GetConversation(
    ::grpc::ServerContext* /*context*/,
    const im::message::GetConversationRequest* request,
    im::message::GetConversationResponse* response) {
    if (!require_service(message_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "GetConversation request is null");
        return ::grpc::Status::OK;
    }
    if (request->user_a().empty() || request->user_b().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "user_a and user_b are required");
        return ::grpc::Status::OK;
    }

    try {
        const auto messages = message_service_->get_conversation(
            request->user_a(), request->user_b(), request->before_time(), request->limit());
        set_base(response->mutable_base(), im::base::SUCCESS);
        for (const auto& message : messages) {
            fill_message_body(message, response->add_messages());
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status MessageGrpcService::PullOffline(
    ::grpc::ServerContext* /*context*/,
    const im::message::PullOfflineRequest* request,
    im::message::PullOfflineResponse* response) {
    if (!require_service(message_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "PullOffline request is null");
        return ::grpc::Status::OK;
    }
    if (request->receiver_uid().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "receiver_uid is required");
        return ::grpc::Status::OK;
    }

    try {
        const auto messages = message_service_->pull_offline(
            request->receiver_uid(), request->before_time(), request->limit());
        set_base(response->mutable_base(), im::base::SUCCESS);
        for (const auto& message : messages) {
            fill_message_body(message, response->add_messages());
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status MessageGrpcService::MarkDelivered(
    ::grpc::ServerContext* /*context*/,
    const im::message::MarkMessageDeliveredRequest* request,
    im::message::MarkMessageDeliveredResponse* response) {
    if (!require_service(message_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "MarkDelivered request is null");
        return ::grpc::Status::OK;
    }
    if (request->msg_id() == 0) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "msg_id is required");
        return ::grpc::Status::OK;
    }

    try {
        const bool marked = message_service_->mark_delivered(
            request->msg_id(),
            request->delivered_time() > 0 ? request->delivered_time() : now_ms());
        response->set_marked(marked);
        set_base(response->mutable_base(),
                 marked ? im::base::SUCCESS : im::base::NOT_FOUND,
                 marked ? "" : "Message not found");
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status MessageGrpcService::MarkRead(
    ::grpc::ServerContext* /*context*/,
    const im::message::MarkMessageReadRequest* request,
    im::message::MarkMessageReadResponse* response) {
    if (!require_service(message_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "MarkRead request is null");
        return ::grpc::Status::OK;
    }
    if (request->msg_id() == 0) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "msg_id is required");
        return ::grpc::Status::OK;
    }

    try {
        const bool marked = message_service_->mark_read(
            request->msg_id(),
            request->read_time() > 0 ? request->read_time() : now_ms());
        response->set_marked(marked);
        set_base(response->mutable_base(),
                 marked ? im::base::SUCCESS : im::base::NOT_FOUND,
                 marked ? "" : "Message not found");
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

}  // namespace im::service::message
