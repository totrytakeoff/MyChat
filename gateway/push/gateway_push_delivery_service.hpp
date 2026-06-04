#ifndef GATEWAY_PUSH_DELIVERY_SERVICE_HPP
#define GATEWAY_PUSH_DELIVERY_SERVICE_HPP

#include <grpcpp/support/status.h>

#include "../../common/proto/push.grpc.pb.h"
#include "../../services/push/push_runtime.hpp"

namespace grpc {
class ServerContext;
}

namespace im::gateway {

// Internal gRPC adapter exposing Gateway-owned WebSocket delivery primitives to
// a standalone Push server. This service does not own fanout policy or payload
// construction; it only bridges session lookup, payload send, and delivery
// marking.
class GatewayPushDeliveryService final
    : public im::push::GatewayPushDeliveryService::Service {
public:
    GatewayPushDeliveryService(
        im::service::push::PushSessionProvider* session_provider,
        im::service::push::PushPayloadSender* payload_sender,
        im::service::push::PushDeliveryMarker* delivery_marker);

    ::grpc::Status ListUserSessions(
        ::grpc::ServerContext* context,
        const im::push::ListUserSessionsRequest* request,
        im::push::ListUserSessionsResponse* response) override;

    ::grpc::Status SendSessionPayload(
        ::grpc::ServerContext* context,
        const im::push::SendSessionPayloadRequest* request,
        im::push::SendSessionPayloadResponse* response) override;

    ::grpc::Status MarkMessageDelivered(
        ::grpc::ServerContext* context,
        const im::push::MarkMessageDeliveredRequest* request,
        im::push::MarkMessageDeliveredResponse* response) override;

private:
    im::service::push::PushSessionProvider* session_provider_;
    im::service::push::PushPayloadSender* payload_sender_;
    im::service::push::PushDeliveryMarker* delivery_marker_;
};

} // namespace im::gateway

#endif // GATEWAY_PUSH_DELIVERY_SERVICE_HPP
