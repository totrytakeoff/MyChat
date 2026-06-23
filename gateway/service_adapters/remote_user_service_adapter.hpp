#ifndef GATEWAY_SERVICE_ADAPTERS_REMOTE_USER_SERVICE_ADAPTER_HPP
#define GATEWAY_SERVICE_ADAPTERS_REMOTE_USER_SERVICE_ADAPTER_HPP

#include <chrono>
#include <memory>
#include <string>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "user.grpc.pb.h"
#include "user_service_adapter.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class UserRpcClient {
public:
    virtual ~UserRpcClient() = default;

    virtual ::grpc::Status forward_packet(
        ::grpc::ClientContext* context,
        const im::user::UserPacketRequest& request,
        im::user::UserPacketResponse* response) = 0;
};

class RemoteUserServiceAdapter final : public UserServiceAdapter {
public:
    explicit RemoteUserServiceAdapter(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteUserServiceAdapter(
        std::unique_ptr<UserRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    im::user::UserPacketResponse forward(
        const im::user::UserPacketRequest& packet) override;

private:
    void apply_deadline(::grpc::ClientContext& context) const;

    std::unique_ptr<UserRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace im::gateway

#endif  // GATEWAY_SERVICE_ADAPTERS_REMOTE_USER_SERVICE_ADAPTER_HPP
