#ifndef SERVICES_PUSH_PUSH_SERVER_ADAPTERS_HPP
#define SERVICES_PUSH_PUSH_SERVER_ADAPTERS_HPP

#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "../../common/proto/push.grpc.pb.h"
#include "push_runtime.hpp"

namespace grpc {
class ClientContext;
}

namespace im::service::push {

// Transitional standalone-server adapters.
//
// The current Gateway process still owns live WebSocket sessions. Until a
// cross-process session/payload channel is added, the standalone Push server
// should accept NotifyUser RPCs without pretending it can deliver to Gateway
// sessions.
class EmptyPushSessionProvider final : public PushSessionProvider {
public:
    std::vector<PushSessionInfo> get_sessions(const std::string& receiver_uid) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

class NoopPushPayloadSender final : public PushPayloadSender {
public:
    bool send_payload(const std::string& session_id,
                      const std::string& payload) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

class NoopPushDeliveryMarker final : public PushDeliveryMarker {
public:
    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

class GatewayDeliveryRpcClient {
public:
    virtual ~GatewayDeliveryRpcClient() = default;

    virtual ::grpc::Status list_user_sessions(
        ::grpc::ClientContext* context,
        const im::push::ListUserSessionsRequest& request,
        im::push::ListUserSessionsResponse* response) = 0;

    virtual ::grpc::Status send_session_payload(
        ::grpc::ClientContext* context,
        const im::push::SendSessionPayloadRequest& request,
        im::push::SendSessionPayloadResponse* response) = 0;

    virtual ::grpc::Status mark_message_delivered(
        ::grpc::ClientContext* context,
        const im::push::MarkMessageDeliveredRequest& request,
        im::push::MarkMessageDeliveredResponse* response) = 0;
};

class RemoteGatewayPushSessionProvider final : public PushSessionProvider {
public:
    explicit RemoteGatewayPushSessionProvider(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteGatewayPushSessionProvider(
        std::shared_ptr<GatewayDeliveryRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    std::vector<PushSessionInfo> get_sessions(const std::string& receiver_uid) override;

private:
    std::shared_ptr<GatewayDeliveryRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

class RemoteGatewayPushPayloadSender final : public PushPayloadSender {
public:
    explicit RemoteGatewayPushPayloadSender(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteGatewayPushPayloadSender(
        std::shared_ptr<GatewayDeliveryRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    bool send_payload(const std::string& session_id,
                      const std::string& payload) override;

private:
    std::shared_ptr<GatewayDeliveryRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

class RemoteGatewayPushDeliveryMarker final : public PushDeliveryMarker {
public:
    explicit RemoteGatewayPushDeliveryMarker(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteGatewayPushDeliveryMarker(
        std::shared_ptr<GatewayDeliveryRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override;

private:
    std::shared_ptr<GatewayDeliveryRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::service::push

#endif // SERVICES_PUSH_PUSH_SERVER_ADAPTERS_HPP
