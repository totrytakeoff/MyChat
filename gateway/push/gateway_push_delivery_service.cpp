#include "gateway_push_delivery_service.hpp"

#include <chrono>
#include <exception>
#include <string>

#include "../../common/proto/base.pb.h"

namespace im::gateway {

namespace {

int64_t to_epoch_ms(const std::chrono::system_clock::time_point& time_point) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch()).count();
}

void set_base(im::base::BaseResponse* base,
              im::base::ErrorCode code,
              const std::string& message = "") {
    base->set_error_code(code);
    base->set_error_message(message);
}

} // namespace

GatewayPushDeliveryService::GatewayPushDeliveryService(
    im::service::push::PushSessionProvider* session_provider,
    im::service::push::PushPayloadSender* payload_sender,
    im::service::push::PushDeliveryMarker* delivery_marker)
    : session_provider_(session_provider)
    , payload_sender_(payload_sender)
    , delivery_marker_(delivery_marker)
{}

::grpc::Status GatewayPushDeliveryService::ListUserSessions(
    ::grpc::ServerContext* /*context*/,
    const im::push::ListUserSessionsRequest* request,
    im::push::ListUserSessionsResponse* response) {
    auto* base = response->mutable_base();
    if (!request) {
        set_base(base, im::base::INVALID_REQUEST, "ListUserSessions request is null");
        return ::grpc::Status::OK;
    }
    if (!session_provider_) {
        set_base(base, im::base::SERVER_ERROR, "Push session provider is not configured");
        return ::grpc::Status::OK;
    }
    if (request->receiver_uid().empty()) {
        set_base(base, im::base::PARAM_ERROR, "receiver_uid is required");
        return ::grpc::Status::OK;
    }

    try {
        auto sessions = session_provider_->get_sessions(request->receiver_uid());
        for (const auto& session : sessions) {
            auto* proto_session = response->add_sessions();
            proto_session->set_session_id(session.session_id);
            proto_session->set_platform(session.platform);
            proto_session->set_connect_time_ms(to_epoch_ms(session.connect_time));
        }
        set_base(base, im::base::SUCCESS);
    } catch (const std::exception& e) {
        set_base(base, im::base::SERVER_ERROR, e.what());
    }
    return ::grpc::Status::OK;
}

::grpc::Status GatewayPushDeliveryService::SendSessionPayload(
    ::grpc::ServerContext* /*context*/,
    const im::push::SendSessionPayloadRequest* request,
    im::push::SendSessionPayloadResponse* response) {
    auto* base = response->mutable_base();
    if (!request) {
        set_base(base, im::base::INVALID_REQUEST, "SendSessionPayload request is null");
        return ::grpc::Status::OK;
    }
    if (!payload_sender_) {
        set_base(base, im::base::SERVER_ERROR, "Push payload sender is not configured");
        return ::grpc::Status::OK;
    }
    if (request->session_id().empty() || request->payload().empty()) {
        set_base(base, im::base::PARAM_ERROR, "session_id and payload are required");
        return ::grpc::Status::OK;
    }

    try {
        const bool accepted =
            payload_sender_->send_payload(request->session_id(), request->payload());
        response->set_accepted(accepted);
        set_base(base, im::base::SUCCESS);
    } catch (const std::exception& e) {
        set_base(base, im::base::SERVER_ERROR, e.what());
    }
    return ::grpc::Status::OK;
}

::grpc::Status GatewayPushDeliveryService::MarkMessageDelivered(
    ::grpc::ServerContext* /*context*/,
    const im::push::MarkMessageDeliveredRequest* request,
    im::push::MarkMessageDeliveredResponse* response) {
    auto* base = response->mutable_base();
    if (!request) {
        set_base(base, im::base::INVALID_REQUEST, "MarkMessageDelivered request is null");
        return ::grpc::Status::OK;
    }
    if (!delivery_marker_) {
        set_base(base, im::base::SERVER_ERROR, "Push delivery marker is not configured");
        return ::grpc::Status::OK;
    }
    if (request->msg_id() == 0 || request->delivered_time() <= 0) {
        set_base(base, im::base::PARAM_ERROR, "msg_id and delivered_time are required");
        return ::grpc::Status::OK;
    }

    try {
        const bool marked =
            delivery_marker_->mark_delivered(request->msg_id(), request->delivered_time());
        response->set_marked(marked);
        set_base(base, im::base::SUCCESS);
    } catch (const std::exception& e) {
        set_base(base, im::base::SERVER_ERROR, e.what());
    }
    return ::grpc::Status::OK;
}

} // namespace im::gateway
