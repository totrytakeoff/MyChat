#include "remote_message_service_adapter.hpp"

#include <chrono>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "../../common/utils/log_manager.hpp"

namespace im::gateway {
namespace {

class GrpcMessageRpcClient final : public MessageRpcClient {
public:
    explicit GrpcMessageRpcClient(const std::string& endpoint)
        : stub_(im::message::MessageService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()))) {}

    ::grpc::Status forward_packet(::grpc::ClientContext* context,
                                  const im::message::MessagePacketRequest& request,
                                  im::message::MessagePacketResponse* response) override {
        return stub_->ForwardPacket(context, request, response);
    }

private:
    std::unique_ptr<im::message::MessageService::Stub> stub_;
};

}  // namespace

RemoteMessageServiceAdapter::RemoteMessageServiceAdapter(const std::string& endpoint,
                                                         std::chrono::milliseconds timeout)
    : RemoteMessageServiceAdapter(std::make_unique<GrpcMessageRpcClient>(endpoint), timeout) {}

RemoteMessageServiceAdapter::RemoteMessageServiceAdapter(std::unique_ptr<MessageRpcClient> client,
                                                         std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout) {
    im::utils::LogManager::SetLogToFile("remote_message_adapter",
                                        "logs/remote_message_adapter.log");
    logger_ = im::utils::LogManager::GetLogger("remote_message_adapter");
}

im::message::MessagePacketResponse RemoteMessageServiceAdapter::forward(
        const im::message::MessagePacketRequest& packet) {
    im::message::MessagePacketResponse response;
    response.mutable_header()->CopyFrom(packet.header());
    if (!client_) {
        response.mutable_base()->set_error_code(im::base::SERVER_ERROR);
        response.mutable_base()->set_error_message(
                "Remote Message service adapter is not configured");
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
        logger_->warn("Remote message packet forward RPC failed cmd_id={}, type={}: {}",
                      packet.header().cmd_id(), packet.type_name(), status.error_message());
    }
    return response;
}

void RemoteMessageServiceAdapter::apply_deadline(::grpc::ClientContext& context) const {
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }
}

}  // namespace im::gateway
