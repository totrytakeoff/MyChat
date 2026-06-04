#include "push_grpc_service.hpp"

#include <exception>
#include <string>

#include <grpcpp/server_context.h>

#include "base.pb.h"

namespace im::service::push {

PushGrpcService::PushGrpcService(PushNotifier* notifier)
    : notifier_(notifier)
{}

::grpc::Status PushGrpcService::NotifyUser(::grpc::ServerContext* /*context*/,
                                           const im::push::NotifyUserRequest* request,
                                           im::push::NotifyUserResponse* response) {
    auto* base = response->mutable_base();

    if (!request) {
        base->set_error_code(im::base::INVALID_REQUEST);
        base->set_error_message("NotifyUser request is null");
        return ::grpc::Status::OK;
    }

    if (!notifier_) {
        base->set_error_code(im::base::SERVER_ERROR);
        base->set_error_message("Push notifier is not configured");
        return ::grpc::Status::OK;
    }

    if (request->receiver_uid().empty() || request->msg_id() == 0) {
        base->set_error_code(im::base::PARAM_ERROR);
        base->set_error_message("receiver_uid and msg_id are required");
        return ::grpc::Status::OK;
    }

    try {
        notifier_->notify_user(request->receiver_uid(),
                               request->msg_id(),
                               request->content());
        base->set_error_code(im::base::SUCCESS);
        base->set_error_message("");
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        base->set_error_code(im::base::SERVER_ERROR);
        base->set_error_message(e.what());
        return ::grpc::Status::OK;
    }
}

} // namespace im::service::push
