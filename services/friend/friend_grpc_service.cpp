#include "friend_grpc_service.hpp"

#include "friend_service.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>

#include <grpcpp/server_context.h>

#include "base.pb.h"
#include "friend.hpp"
#include "friend.pb.h"

namespace im::service::friend_ {

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void set_base(im::base::BaseResponse* base,
              im::base::ErrorCode code,
              const std::string& message = {}) {
    base->set_error_code(code);
    base->set_error_message(message);
}

im::base::ErrorCode friend_error_code(const std::string& error_code) {
    if (error_code == "EMPTY_UID" ||
        error_code == "SELF_REQUEST" ||
        error_code == "NOT_PENDING") {
        return im::base::PARAM_ERROR;
    }
    if (error_code == "TARGET_NOT_FOUND" || error_code == "NOT_FOUND") {
        return im::base::NOT_FOUND;
    }
    if (error_code == "ALREADY_EXISTS") {
        return im::base::ALREADY_EXISTS;
    }
    if (error_code == "FORBIDDEN") {
        return im::base::PERMISSION_DENIED;
    }
    return im::base::SERVER_ERROR;
}

im::friend_::FriendRequestStatus to_proto_status(FriendStatus status) {
    switch (status) {
    case FriendStatus::PENDING:
        return im::friend_::PENDING;
    case FriendStatus::ACCEPTED:
        return im::friend_::ACCEPTED;
    case FriendStatus::REJECTED:
        return im::friend_::REJECTED;
    default:
        return im::friend_::PENDING;
    }
}

void fill_friend_info(const FriendInfoDTO& dto, im::friend_::FriendInfo* info) {
    info->set_uid(dto.friend_uid);
    info->set_add_time(dto.created_at);
    info->set_friend_id(dto.friend_id);
    info->set_nickname(dto.nickname);
    info->set_status(to_proto_status(dto.status));
}

void fill_friend_request(const FriendInfoDTO& dto, im::friend_::FriendRequest* request) {
    request->set_request_id(std::to_string(dto.friend_id));
    request->set_friend_id(dto.friend_id);
    request->set_from_uid(dto.friend_uid);
    request->set_status(to_proto_status(dto.status));
    request->set_create_time(dto.created_at);
    request->set_nickname(dto.nickname);
}

template <typename Response>
bool require_service(FriendService* service, Response* response) {
    if (!response) {
        return false;
    }
    if (!service) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR,
                 "FriendService is not available");
        return false;
    }
    return true;
}

}  // namespace

FriendGrpcService::FriendGrpcService(FriendService* friend_service)
    : friend_service_(friend_service) {}

::grpc::Status FriendGrpcService::SendRequest(
    ::grpc::ServerContext* /*context*/,
    const im::friend_::AddFriendRequest* request,
    im::friend_::AddFriendResponse* response) {
    if (!require_service(friend_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "AddFriend request is null");
        return ::grpc::Status::OK;
    }

    try {
        FriendRequest service_request;
        service_request.requester_uid = request->header().from_uid();
        service_request.target_uid = request->to_uid();
        service_request.now_ms = request->header().timestamp() > 0
            ? static_cast<int64_t>(request->header().timestamp())
            : now_ms();

        const auto result = friend_service_->send_request(service_request);
        if (!result.ok) {
            set_base(response->mutable_base(),
                     friend_error_code(result.error_code),
                     result.message);
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        auto pending = friend_service_->get_pending_requests(service_request.target_uid);
        for (const auto& item : pending) {
            if (item.friend_uid == service_request.requester_uid) {
                fill_friend_request(item, response->mutable_request());
                response->mutable_request()->set_to_uid(service_request.target_uid);
                break;
            }
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status FriendGrpcService::RespondToRequest(
    ::grpc::ServerContext* /*context*/,
    const im::friend_::HandleFriendRequest* request,
    im::friend_::HandleFriendResponse* response) {
    if (!require_service(friend_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "HandleFriend request is null");
        return ::grpc::Status::OK;
    }

    uint64_t friend_id = 0;
    try {
        friend_id = std::stoull(request->request_id());
    } catch (...) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "request_id must be a numeric friend id");
        return ::grpc::Status::OK;
    }

    try {
        const auto result = friend_service_->respond_to_request(
            friend_id, request->header().from_uid(), request->accept());
        if (!result.ok) {
            set_base(response->mutable_base(),
                     friend_error_code(result.error_code),
                     result.message);
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status FriendGrpcService::GetFriends(
    ::grpc::ServerContext* /*context*/,
    const im::friend_::GetFriendListRequest* request,
    im::friend_::GetFriendListResponse* response) {
    if (!require_service(friend_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request || request->header().from_uid().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "from_uid is required");
        return ::grpc::Status::OK;
    }

    try {
        const auto friends = friend_service_->get_friends(request->header().from_uid());
        set_base(response->mutable_base(), im::base::SUCCESS);
        for (const auto& item : friends) {
            fill_friend_info(item, response->add_friends());
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status FriendGrpcService::GetPendingRequests(
    ::grpc::ServerContext* /*context*/,
    const im::friend_::GetFriendRequestsRequest* request,
    im::friend_::GetFriendRequestsResponse* response) {
    if (!require_service(friend_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request || request->header().from_uid().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "from_uid is required");
        return ::grpc::Status::OK;
    }

    try {
        const auto requests = friend_service_->get_pending_requests(request->header().from_uid());
        set_base(response->mutable_base(), im::base::SUCCESS);
        for (const auto& item : requests) {
            auto* out = response->add_requests();
            fill_friend_request(item, out);
            out->set_to_uid(request->header().from_uid());
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

}  // namespace im::service::friend_
