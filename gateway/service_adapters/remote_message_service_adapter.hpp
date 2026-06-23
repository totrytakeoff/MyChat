#ifndef GATEWAY_SERVICE_ADAPTERS_REMOTE_MESSAGE_SERVICE_ADAPTER_HPP
#define GATEWAY_SERVICE_ADAPTERS_REMOTE_MESSAGE_SERVICE_ADAPTER_HPP

#include <chrono>
#include <memory>
#include <string>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "message.grpc.pb.h"
#include "message_service_adapter.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class MessageRpcClient {
public:
    virtual ~MessageRpcClient() = default;

    virtual ::grpc::Status forward_packet(
        ::grpc::ClientContext* context,
        const im::message::MessagePacketRequest& request,
        im::message::MessagePacketResponse* response) = 0;
};

class RemoteMessageServiceAdapter final : public MessageServiceAdapter {
public:
    explicit RemoteMessageServiceAdapter(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteMessageServiceAdapter(
        std::unique_ptr<MessageRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    im::message::MessagePacketResponse forward(
        const im::message::MessagePacketRequest& packet) override;

private:
    void apply_deadline(::grpc::ClientContext& context) const;

    std::unique_ptr<MessageRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace im::gateway

#endif  // GATEWAY_SERVICE_ADAPTERS_REMOTE_MESSAGE_SERVICE_ADAPTER_HPP
