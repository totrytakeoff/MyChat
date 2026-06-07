#include "remote_friend_client.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "base.pb.h"
#include "friend.hpp"
#include "../../common/utils/log_manager.hpp"

namespace im::gateway {

namespace {

class GrpcFriendRpcClient final : public FriendRpcClient {
public:
    explicit GrpcFriendRpcClient(const std::string& endpoint)
        : stub_(im::friend_::FriendService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()))) {}

    ::grpc::Status send_request(::grpc::ClientContext* context,
                                const im::friend_::AddFriendRequest& request,
                                im::friend_::AddFriendResponse* response) override {
        return stub_->SendRequest(context, request, response);
    }

    ::grpc::Status respond_to_request(
        ::grpc::ClientContext* context,
        const im::friend_::HandleFriendRequest& request,
        im::friend_::HandleFriendResponse* response) override {
        return stub_->RespondToRequest(context, request, response);
    }

    ::grpc::Status get_friends(::grpc::ClientContext* context,
                               const im::friend_::GetFriendListRequest& request,
                               im::friend_::GetFriendListResponse* response) override {
        return stub_->GetFriends(context, request, response);
    }

    ::grpc::Status get_pending_requests(
        ::grpc::ClientContext* context,
        const im::friend_::GetFriendRequestsRequest& request,
        im::friend_::GetFriendRequestsResponse* response) override {
        return stub_->GetPendingRequests(context, request, response);
    }

private:
    std::unique_ptr<im::friend_::FriendService::Stub> stub_;
};

im::service::friend_::FriendStatus to_service_status(
    im::friend_::FriendRequestStatus status) {
    switch (status) {
    case im::friend_::ACCEPTED:
        return im::service::friend_::FriendStatus::ACCEPTED;
    case im::friend_::REJECTED:
        return im::service::friend_::FriendStatus::REJECTED;
    case im::friend_::PENDING:
    default:
        return im::service::friend_::FriendStatus::PENDING;
    }
}

im::service::friend_::FriendInfoDTO to_info(const im::friend_::FriendInfo& info) {
    im::service::friend_::FriendInfoDTO dto;
    dto.friend_id = info.friend_id();
    dto.friend_uid = info.uid();
    dto.nickname = info.nickname();
    dto.status = to_service_status(info.status());
    dto.created_at = info.add_time();
    return dto;
}

im::service::friend_::FriendInfoDTO to_info(const im::friend_::FriendRequest& request) {
    im::service::friend_::FriendInfoDTO dto;
    dto.friend_id = request.friend_id();
    dto.friend_uid = request.from_uid();
    dto.status = to_service_status(request.status());
    dto.created_at = request.create_time();
    return dto;
}

std::string friend_error_code(im::base::ErrorCode code) {
    switch (code) {
    case im::base::PARAM_ERROR:
        return "PARAM_ERROR";
    case im::base::NOT_FOUND:
        return "NOT_FOUND";
    case im::base::ALREADY_EXISTS:
        return "ALREADY_EXISTS";
    case im::base::PERMISSION_DENIED:
        return "FORBIDDEN";
    case im::base::SERVER_ERROR:
        return "REMOTE_FRIEND_SERVER_ERROR";
    default:
        return "REMOTE_FRIEND_ERROR";
    }
}

}  // namespace

RemoteFriendClient::RemoteFriendClient(const std::string& endpoint,
                                       std::chrono::milliseconds timeout)
    : RemoteFriendClient(std::make_unique<GrpcFriendRpcClient>(endpoint), timeout) {}

RemoteFriendClient::RemoteFriendClient(std::unique_ptr<FriendRpcClient> client,
                                       std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout) {
    im::utils::LogManager::SetLogToFile("remote_friend_client",
                                        "logs/remote_friend_client.log");
    logger_ = im::utils::LogManager::GetLogger("remote_friend_client");
}

im::service::friend_::FriendResult RemoteFriendClient::send_request(
    const im::service::friend_::FriendRequest& request) {
    im::service::friend_::FriendResult result;
    if (!client_) {
        result.error_code = "REMOTE_FRIEND_UNAVAILABLE";
        result.message = "Remote Friend client is not configured";
        return result;
    }

    im::friend_::AddFriendRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(request.requester_uid);
    rpc_request.mutable_header()->set_timestamp(static_cast<uint64_t>(request.now_ms));
    rpc_request.set_to_uid(request.target_uid);

    im::friend_::AddFriendResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->send_request(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_FRIEND_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote friend send request RPC failed {} -> {}: {}",
                      request.requester_uid, request.target_uid, status.error_message());
        return result;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = friend_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }

    result.ok = true;
    result.message = "Friend request sent";
    return result;
}

im::service::friend_::FriendResult RemoteFriendClient::respond_to_request(
    uint64_t friend_id,
    const std::string& uid,
    bool accept) {
    im::service::friend_::FriendResult result;
    if (!client_) {
        result.error_code = "REMOTE_FRIEND_UNAVAILABLE";
        result.message = "Remote Friend client is not configured";
        return result;
    }

    im::friend_::HandleFriendRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(uid);
    rpc_request.set_request_id(std::to_string(friend_id));
    rpc_request.set_accept(accept);

    im::friend_::HandleFriendResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->respond_to_request(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_FRIEND_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote friend respond RPC failed friend_id={} user={}: {}",
                      friend_id, uid, status.error_message());
        return result;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = friend_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }

    result.ok = true;
    result.message = accept ? "Friend request accepted" : "Friend request rejected";
    return result;
}

std::vector<im::service::friend_::FriendInfoDTO> RemoteFriendClient::get_friends(
    const std::string& uid) {
    if (!client_) {
        logger_->warn("Remote friend list lookup skipped: RPC client is not configured");
        return {};
    }

    im::friend_::GetFriendListRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(uid);

    im::friend_::GetFriendListResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->get_friends(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote friend list RPC failed for {}: {}",
                      uid, status.error_message());
        return {};
    }
    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Remote friend list RPC returned business error: {}",
                      rpc_response.base().error_message());
        return {};
    }

    std::vector<im::service::friend_::FriendInfoDTO> friends;
    friends.reserve(static_cast<size_t>(rpc_response.friends_size()));
    for (const auto& friend_info : rpc_response.friends()) {
        friends.push_back(to_info(friend_info));
    }
    return friends;
}

std::vector<im::service::friend_::FriendInfoDTO> RemoteFriendClient::get_pending_requests(
    const std::string& uid) {
    if (!client_) {
        logger_->warn("Remote pending friend lookup skipped: RPC client is not configured");
        return {};
    }

    im::friend_::GetFriendRequestsRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(uid);

    im::friend_::GetFriendRequestsResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->get_pending_requests(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote pending friend RPC failed for {}: {}",
                      uid, status.error_message());
        return {};
    }
    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Remote pending friend RPC returned business error: {}",
                      rpc_response.base().error_message());
        return {};
    }

    std::vector<im::service::friend_::FriendInfoDTO> requests;
    requests.reserve(static_cast<size_t>(rpc_response.requests_size()));
    for (const auto& request : rpc_response.requests()) {
        requests.push_back(to_info(request));
    }
    return requests;
}

void RemoteFriendClient::apply_deadline(::grpc::ClientContext& context) const {
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }
}

}  // namespace im::gateway
