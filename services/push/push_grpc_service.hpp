#ifndef SERVICES_PUSH_PUSH_GRPC_SERVICE_HPP
#define SERVICES_PUSH_PUSH_GRPC_SERVICE_HPP

#include <grpcpp/support/status.h>

#include "push.grpc.pb.h"
#include "push_notifier.hpp"

namespace grpc {
class ServerContext;
}

namespace im::service::push {

// gRPC adapter for the PushNotifier boundary. It keeps the transport contract
// separate from PushRuntime so Gateway can switch from in-process calls to RPC
// without changing message entry points.
class PushGrpcService final : public im::push::PushService::Service {
public:
    explicit PushGrpcService(PushNotifier* notifier);

    ::grpc::Status NotifyUser(::grpc::ServerContext* context,
                              const im::push::NotifyUserRequest* request,
                              im::push::NotifyUserResponse* response) override;

private:
    PushNotifier* notifier_;
};

} // namespace im::service::push

#endif // SERVICES_PUSH_PUSH_GRPC_SERVICE_HPP
