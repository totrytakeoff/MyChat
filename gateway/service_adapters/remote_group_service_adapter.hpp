#ifndef GATEWAY_SERVICE_ADAPTERS_REMOTE_GROUP_SERVICE_ADAPTER_HPP
#define GATEWAY_SERVICE_ADAPTERS_REMOTE_GROUP_SERVICE_ADAPTER_HPP

#include <chrono>
#include <memory>
#include <string>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "group.grpc.pb.h"
#include "group_service_adapter.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class GroupRpcClient {
public:
    virtual ~GroupRpcClient() = default;

    virtual ::grpc::Status forward_packet(
        ::grpc::ClientContext* context,
        const im::group::GroupPacketRequest& request,
        im::group::GroupPacketResponse* response) = 0;
};

class RemoteGroupServiceAdapter final : public GroupServiceAdapter {
public:
    explicit RemoteGroupServiceAdapter(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteGroupServiceAdapter(
        std::unique_ptr<GroupRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    im::group::GroupPacketResponse forward(
        const im::group::GroupPacketRequest& packet) override;

private:
    void apply_deadline(::grpc::ClientContext& context) const;

    std::unique_ptr<GroupRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace im::gateway

#endif  // GATEWAY_SERVICE_ADAPTERS_REMOTE_GROUP_SERVICE_ADAPTER_HPP
