#include "remote_push_notifier.hpp"

#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "../../common/proto/base.pb.h"
#include "../../common/utils/log_manager.hpp"

namespace im::gateway {

namespace {

class GrpcPushRpcClient final : public PushRpcClient {
public:
    explicit GrpcPushRpcClient(const std::string& endpoint)
        : stub_(im::push::PushService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials())))
    {}

    ::grpc::Status notify_user(::grpc::ClientContext* context,
                               const im::push::NotifyUserRequest& request,
                               im::push::NotifyUserResponse* response) override {
        return stub_->NotifyUser(context, request, response);
    }

private:
    std::unique_ptr<im::push::PushService::Stub> stub_;
};

} // namespace

RemotePushNotifier::RemotePushNotifier(const std::string& endpoint,
                                       std::chrono::milliseconds timeout)
    : RemotePushNotifier(std::make_unique<GrpcPushRpcClient>(endpoint), timeout)
{}

RemotePushNotifier::RemotePushNotifier(std::unique_ptr<PushRpcClient> client,
                                       std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout)
{
    im::utils::LogManager::SetLogToFile("remote_push_notifier",
                                        "logs/remote_push_notifier.log");
    logger_ = im::utils::LogManager::GetLogger("remote_push_notifier");
}

void RemotePushNotifier::notify_user(const std::string& receiver_uid,
                                     uint64_t msg_id,
                                     const std::string& content,
                                     const im::service::push::PushContext& push_context) {
    if (!client_) {
        logger_->warn("Remote push skipped: RPC client is not configured");
        return;
    }

    im::push::NotifyUserRequest request;
    request.set_receiver_uid(receiver_uid);
    request.set_msg_id(msg_id);
    request.set_content(content);
    request.set_sender_uid(push_context.sender_uid);
    request.set_conversation_type(push_context.conversation_type);
    request.set_conversation_id(push_context.conversation_id);

    im::push::NotifyUserResponse response;
    ::grpc::ClientContext context;
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }

    auto status = client_->notify_user(&context, request, &response);
    if (!status.ok()) {
        logger_->warn("Remote push RPC failed for receiver {}, msg {}: {}",
                      receiver_uid, msg_id, status.error_message());
        return;
    }

    if (response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Remote push rejected for receiver {}, msg {}: {} ({})",
                      receiver_uid,
                      msg_id,
                      response.base().error_message(),
                      static_cast<int>(response.base().error_code()));
    }
}

} // namespace im::gateway
