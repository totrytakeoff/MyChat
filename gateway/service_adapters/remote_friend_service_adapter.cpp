#include "remote_friend_service_adapter.hpp"

#include <chrono>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "../../common/utils/log_manager.hpp"

namespace im::gateway {
namespace {

class GrpcFriendRpcClient final : public FriendRpcClient {
public:
    explicit GrpcFriendRpcClient(const std::string& endpoint)
        : stub_(im::friend_::FriendService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()))) {}

    ::grpc::Status forward_packet(::grpc::ClientContext* context,
                                  const im::friend_::FriendPacketRequest& request,
                                  im::friend_::FriendPacketResponse* response) override {
        return stub_->ForwardPacket(context, request, response);
    }

private:
    std::unique_ptr<im::friend_::FriendService::Stub> stub_;
};

}  // namespace

RemoteFriendServiceAdapter::RemoteFriendServiceAdapter(const std::string& endpoint,
                                                       std::chrono::milliseconds timeout)
    : RemoteFriendServiceAdapter(std::make_unique<GrpcFriendRpcClient>(endpoint), timeout) {}

RemoteFriendServiceAdapter::RemoteFriendServiceAdapter(std::unique_ptr<FriendRpcClient> client,
                                                       std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout) {
    im::utils::LogManager::SetLogToFile("remote_friend_adapter",
                                        "logs/remote_friend_adapter.log");
    logger_ = im::utils::LogManager::GetLogger("remote_friend_adapter");
}

im::friend_::FriendPacketResponse RemoteFriendServiceAdapter::forward(
        const im::friend_::FriendPacketRequest& packet) {
    im::friend_::FriendPacketResponse response;
    response.mutable_header()->CopyFrom(packet.header());
    if (!client_) {
        response.mutable_base()->set_error_code(im::base::SERVER_ERROR);
        response.mutable_base()->set_error_message(
                "Remote Friend service adapter is not configured");
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
        logger_->warn("Remote friend packet forward RPC failed cmd_id={}, type={}: {}",
                      packet.header().cmd_id(), packet.type_name(), status.error_message());
    }
    return response;
}

void RemoteFriendServiceAdapter::apply_deadline(::grpc::ClientContext& context) const {
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }
}

}  // namespace im::gateway
