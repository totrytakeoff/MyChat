#include "remote_group_service_adapter.hpp"

#include <chrono>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "../../common/utils/log_manager.hpp"

namespace im::gateway {
namespace {

class GrpcGroupRpcClient final : public GroupRpcClient {
public:
    explicit GrpcGroupRpcClient(const std::string& endpoint)
        : stub_(im::group::GroupService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()))) {}

    ::grpc::Status forward_packet(::grpc::ClientContext* context,
                                  const im::group::GroupPacketRequest& request,
                                  im::group::GroupPacketResponse* response) override {
        return stub_->ForwardPacket(context, request, response);
    }

private:
    std::unique_ptr<im::group::GroupService::Stub> stub_;
};

}  // namespace

RemoteGroupServiceAdapter::RemoteGroupServiceAdapter(const std::string& endpoint,
                                                     std::chrono::milliseconds timeout)
    : RemoteGroupServiceAdapter(std::make_unique<GrpcGroupRpcClient>(endpoint), timeout) {}

RemoteGroupServiceAdapter::RemoteGroupServiceAdapter(std::unique_ptr<GroupRpcClient> client,
                                                     std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout) {
    im::utils::LogManager::SetLogToFile("remote_group_adapter",
                                        "logs/remote_group_adapter.log");
    logger_ = im::utils::LogManager::GetLogger("remote_group_adapter");
}

im::group::GroupPacketResponse RemoteGroupServiceAdapter::forward(
        const im::group::GroupPacketRequest& packet) {
    im::group::GroupPacketResponse response;
    response.mutable_header()->CopyFrom(packet.header());
    if (!client_) {
        response.mutable_base()->set_error_code(im::base::SERVER_ERROR);
        response.mutable_base()->set_error_message(
                "Remote Group service adapter is not configured");
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
        logger_->warn("Remote group packet forward RPC failed cmd_id={}, type={}: {}",
                      packet.header().cmd_id(), packet.type_name(), status.error_message());
    }
    return response;
}

void RemoteGroupServiceAdapter::apply_deadline(::grpc::ClientContext& context) const {
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }
}

}  // namespace im::gateway
