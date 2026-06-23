#ifndef GATEWAY_SERVICE_ADAPTERS_REMOTE_FRIEND_SERVICE_ADAPTER_HPP
#define GATEWAY_SERVICE_ADAPTERS_REMOTE_FRIEND_SERVICE_ADAPTER_HPP

#include <chrono>
#include <memory>
#include <string>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "friend.grpc.pb.h"
#include "friend_service_adapter.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class FriendRpcClient {
public:
    virtual ~FriendRpcClient() = default;

    virtual ::grpc::Status forward_packet(
        ::grpc::ClientContext* context,
        const im::friend_::FriendPacketRequest& request,
        im::friend_::FriendPacketResponse* response) = 0;
};

class RemoteFriendServiceAdapter final : public FriendServiceAdapter {
public:
    explicit RemoteFriendServiceAdapter(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteFriendServiceAdapter(
        std::unique_ptr<FriendRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    im::friend_::FriendPacketResponse forward(
        const im::friend_::FriendPacketRequest& packet) override;

private:
    void apply_deadline(::grpc::ClientContext& context) const;

    std::unique_ptr<FriendRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace im::gateway

#endif  // GATEWAY_SERVICE_ADAPTERS_REMOTE_FRIEND_SERVICE_ADAPTER_HPP
