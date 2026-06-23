#include "remote_user_service_adapter.hpp"

#include <chrono>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "../../common/utils/log_manager.hpp"

namespace im::gateway {
namespace {

class GrpcUserRpcClient final : public UserRpcClient {
public:
    explicit GrpcUserRpcClient(const std::string& endpoint)
        : stub_(im::user::UserService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()))) {}

    ::grpc::Status forward_packet(::grpc::ClientContext* context,
                                  const im::user::UserPacketRequest& request,
                                  im::user::UserPacketResponse* response) override {
        return stub_->ForwardPacket(context, request, response);
    }

private:
    std::unique_ptr<im::user::UserService::Stub> stub_;
};

}  // namespace

RemoteUserServiceAdapter::RemoteUserServiceAdapter(const std::string& endpoint,
                                                   std::chrono::milliseconds timeout)
    : RemoteUserServiceAdapter(std::make_unique<GrpcUserRpcClient>(endpoint), timeout) {}

RemoteUserServiceAdapter::RemoteUserServiceAdapter(std::unique_ptr<UserRpcClient> client,
                                                   std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout) {
    im::utils::LogManager::SetLogToFile("remote_user_adapter",
                                        "logs/remote_user_adapter.log");
    logger_ = im::utils::LogManager::GetLogger("remote_user_adapter");
}

im::user::UserPacketResponse RemoteUserServiceAdapter::forward(
        const im::user::UserPacketRequest& packet) {
    im::user::UserPacketResponse response;
    response.mutable_header()->CopyFrom(packet.header());
    if (!client_) {
        response.mutable_base()->set_error_code(im::base::SERVER_ERROR);
        response.mutable_base()->set_error_message(
                "Remote User service adapter is not configured");
        return response;
    }

    ::grpc::ClientContext context;
    apply_deadline(context);
    const auto status = client_->forward_packet(&context, packet, &response);
    if (!status.ok()) {
        response.Clear();
        response.mutable_header()->CopyFrom(packet.header());
        response.mutable_base()->set_error_code(im::base::SERVER_ERROR);
        response.mutable_base()->set_error_message(status.error_message());
        logger_->warn("Remote user packet forward RPC failed cmd_id={}, type={}: {}",
                      packet.header().cmd_id(), packet.type_name(), status.error_message());
    }
    return response;
}

void RemoteUserServiceAdapter::apply_deadline(::grpc::ClientContext& context) const {
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }
}

}  // namespace im::gateway
