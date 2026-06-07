#ifndef IM_SERVICE_MESSAGE_MESSAGE_GRPC_SERVICE_HPP
#define IM_SERVICE_MESSAGE_MESSAGE_GRPC_SERVICE_HPP

#include <grpcpp/support/status.h>

#include "message.grpc.pb.h"

namespace grpc {
class ServerContext;
}

namespace im::service::message {

class MessageService;

class MessageGrpcService final : public im::message::MessageService::Service {
public:
    explicit MessageGrpcService(MessageService* message_service);

    ::grpc::Status SendMessage(::grpc::ServerContext* context,
                               const im::message::SendMessageRequest* request,
                               im::message::SendMessageResponse* response) override;

    ::grpc::Status GetConversation(::grpc::ServerContext* context,
                                   const im::message::GetConversationRequest* request,
                                   im::message::GetConversationResponse* response) override;

    ::grpc::Status PullOffline(::grpc::ServerContext* context,
                               const im::message::PullOfflineRequest* request,
                               im::message::PullOfflineResponse* response) override;

    ::grpc::Status MarkDelivered(::grpc::ServerContext* context,
                                 const im::message::MarkMessageDeliveredRequest* request,
                                 im::message::MarkMessageDeliveredResponse* response) override;

    ::grpc::Status MarkRead(::grpc::ServerContext* context,
                            const im::message::MarkMessageReadRequest* request,
                            im::message::MarkMessageReadResponse* response) override;

private:
    MessageService* message_service_;
};

}  // namespace im::service::message

#endif  // IM_SERVICE_MESSAGE_MESSAGE_GRPC_SERVICE_HPP
