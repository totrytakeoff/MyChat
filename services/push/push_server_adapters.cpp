#include "push_server_adapters.hpp"

#include <chrono>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "../../common/proto/base.pb.h"
#include "../../common/utils/log_manager.hpp"

namespace im::service::push {

namespace {

std::shared_ptr<spdlog::logger> adapter_logger() {
    im::utils::LogManager::SetLogToFile("push_server_adapters",
                                        "logs/push_server_adapters.log");
    return im::utils::LogManager::GetLogger("push_server_adapters");
}

class GrpcGatewayDeliveryRpcClient final : public GatewayDeliveryRpcClient {
public:
    explicit GrpcGatewayDeliveryRpcClient(const std::string& endpoint)
        : stub_(im::push::GatewayPushDeliveryService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials())))
    {}

    ::grpc::Status list_user_sessions(
        ::grpc::ClientContext* context,
        const im::push::ListUserSessionsRequest& request,
        im::push::ListUserSessionsResponse* response) override {
        return stub_->ListUserSessions(context, request, response);
    }

    ::grpc::Status send_session_payload(
        ::grpc::ClientContext* context,
        const im::push::SendSessionPayloadRequest& request,
        im::push::SendSessionPayloadResponse* response) override {
        return stub_->SendSessionPayload(context, request, response);
    }

    ::grpc::Status mark_message_delivered(
        ::grpc::ClientContext* context,
        const im::push::MarkMessageDeliveredRequest& request,
        im::push::MarkMessageDeliveredResponse* response) override {
        return stub_->MarkMessageDelivered(context, request, response);
    }

private:
    std::unique_ptr<im::push::GatewayPushDeliveryService::Stub> stub_;
};

void apply_deadline(::grpc::ClientContext& context, std::chrono::milliseconds timeout) {
    if (timeout.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout);
    }
}

std::chrono::system_clock::time_point from_epoch_ms(int64_t epoch_ms) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(epoch_ms));
}

} // namespace

std::vector<PushSessionInfo> EmptyPushSessionProvider::get_sessions(
    const std::string& receiver_uid) {
    if (!logger_) {
        logger_ = adapter_logger();
    }
    logger_->debug("Standalone Push server has no session channel for receiver {}",
                   receiver_uid);
    return {};
}

bool NoopPushPayloadSender::send_payload(const std::string& session_id,
                                         const std::string& payload) {
    if (!logger_) {
        logger_ = adapter_logger();
    }
    logger_->debug("Standalone Push server dropped payload for session {} ({} bytes)",
                   session_id, payload.size());
    return false;
}

bool NoopPushDeliveryMarker::mark_delivered(uint64_t msg_id, int64_t delivered_time) {
    if (!logger_) {
        logger_ = adapter_logger();
    }
    logger_->debug("Standalone Push server did not mark message {} delivered at {}",
                   msg_id, delivered_time);
    return false;
}

RemoteGatewayPushSessionProvider::RemoteGatewayPushSessionProvider(
    const std::string& endpoint,
    std::chrono::milliseconds timeout)
    : RemoteGatewayPushSessionProvider(
          std::make_shared<GrpcGatewayDeliveryRpcClient>(endpoint), timeout)
{}

RemoteGatewayPushSessionProvider::RemoteGatewayPushSessionProvider(
    std::shared_ptr<GatewayDeliveryRpcClient> client,
    std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout)
    , logger_(adapter_logger())
{}

std::vector<PushSessionInfo> RemoteGatewayPushSessionProvider::get_sessions(
    const std::string& receiver_uid) {
    if (!client_) {
        logger_->warn("Remote Gateway session lookup skipped: RPC client is not configured");
        return {};
    }

    im::push::ListUserSessionsRequest request;
    request.set_receiver_uid(receiver_uid);

    im::push::ListUserSessionsResponse response;
    ::grpc::ClientContext context;
    apply_deadline(context, timeout_);

    auto status = client_->list_user_sessions(&context, request, &response);
    if (!status.ok()) {
        logger_->warn("Gateway session lookup RPC failed for receiver {}: {}",
                      receiver_uid, status.error_message());
        return {};
    }
    if (response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Gateway session lookup rejected for receiver {}: {} ({})",
                      receiver_uid,
                      response.base().error_message(),
                      static_cast<int>(response.base().error_code()));
        return {};
    }

    std::vector<PushSessionInfo> sessions;
    sessions.reserve(static_cast<size_t>(response.sessions_size()));
    for (const auto& proto_session : response.sessions()) {
        sessions.push_back({
            .session_id = proto_session.session_id(),
            .platform = proto_session.platform(),
            .connect_time = from_epoch_ms(proto_session.connect_time_ms()),
        });
    }
    return sessions;
}

RemoteGatewayPushPayloadSender::RemoteGatewayPushPayloadSender(
    const std::string& endpoint,
    std::chrono::milliseconds timeout)
    : RemoteGatewayPushPayloadSender(
          std::make_shared<GrpcGatewayDeliveryRpcClient>(endpoint), timeout)
{}

RemoteGatewayPushPayloadSender::RemoteGatewayPushPayloadSender(
    std::shared_ptr<GatewayDeliveryRpcClient> client,
    std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout)
    , logger_(adapter_logger())
{}

bool RemoteGatewayPushPayloadSender::send_payload(const std::string& session_id,
                                                  const std::string& payload) {
    if (!client_) {
        logger_->warn("Remote Gateway payload send skipped: RPC client is not configured");
        return false;
    }

    im::push::SendSessionPayloadRequest request;
    request.set_session_id(session_id);
    request.set_payload(payload);

    im::push::SendSessionPayloadResponse response;
    ::grpc::ClientContext context;
    apply_deadline(context, timeout_);

    auto status = client_->send_session_payload(&context, request, &response);
    if (!status.ok()) {
        logger_->warn("Gateway payload send RPC failed for session {}: {}",
                      session_id, status.error_message());
        return false;
    }
    if (response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Gateway payload send rejected for session {}: {} ({})",
                      session_id,
                      response.base().error_message(),
                      static_cast<int>(response.base().error_code()));
        return false;
    }
    return response.accepted();
}

RemoteGatewayPushDeliveryMarker::RemoteGatewayPushDeliveryMarker(
    const std::string& endpoint,
    std::chrono::milliseconds timeout)
    : RemoteGatewayPushDeliveryMarker(
          std::make_shared<GrpcGatewayDeliveryRpcClient>(endpoint), timeout)
{}

RemoteGatewayPushDeliveryMarker::RemoteGatewayPushDeliveryMarker(
    std::shared_ptr<GatewayDeliveryRpcClient> client,
    std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout)
    , logger_(adapter_logger())
{}

bool RemoteGatewayPushDeliveryMarker::mark_delivered(uint64_t msg_id,
                                                     int64_t delivered_time) {
    if (!client_) {
        logger_->warn("Remote Gateway delivered marking skipped: RPC client is not configured");
        return false;
    }

    im::push::MarkMessageDeliveredRequest request;
    request.set_msg_id(msg_id);
    request.set_delivered_time(delivered_time);

    im::push::MarkMessageDeliveredResponse response;
    ::grpc::ClientContext context;
    apply_deadline(context, timeout_);

    auto status = client_->mark_message_delivered(&context, request, &response);
    if (!status.ok()) {
        logger_->warn("Gateway delivered marking RPC failed for msg {}: {}",
                      msg_id, status.error_message());
        return false;
    }
    if (response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Gateway delivered marking rejected for msg {}: {} ({})",
                      msg_id,
                      response.base().error_message(),
                      static_cast<int>(response.base().error_code()));
        return false;
    }
    return response.marked();
}

} // namespace im::service::push
