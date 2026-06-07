#ifndef GATEWAY_HTTP_REMOTE_FRIEND_CLIENT_HPP
#define GATEWAY_HTTP_REMOTE_FRIEND_CLIENT_HPP

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "friend.grpc.pb.h"
#include "friend_client.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class FriendRpcClient {
public:
    virtual ~FriendRpcClient() = default;

    virtual ::grpc::Status send_request(
        ::grpc::ClientContext* context,
        const im::friend_::AddFriendRequest& request,
        im::friend_::AddFriendResponse* response) = 0;

    virtual ::grpc::Status respond_to_request(
        ::grpc::ClientContext* context,
        const im::friend_::HandleFriendRequest& request,
        im::friend_::HandleFriendResponse* response) = 0;

    virtual ::grpc::Status get_friends(
        ::grpc::ClientContext* context,
        const im::friend_::GetFriendListRequest& request,
        im::friend_::GetFriendListResponse* response) = 0;

    virtual ::grpc::Status get_pending_requests(
        ::grpc::ClientContext* context,
        const im::friend_::GetFriendRequestsRequest& request,
        im::friend_::GetFriendRequestsResponse* response) = 0;
};

class RemoteFriendClient final : public FriendClient {
public:
    explicit RemoteFriendClient(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteFriendClient(
        std::unique_ptr<FriendRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    im::service::friend_::FriendResult send_request(
        const im::service::friend_::FriendRequest& request) override;

    im::service::friend_::FriendResult respond_to_request(
        uint64_t friend_id,
        const std::string& uid,
        bool accept) override;

    std::vector<im::service::friend_::FriendInfoDTO> get_friends(
        const std::string& uid) override;

    std::vector<im::service::friend_::FriendInfoDTO> get_pending_requests(
        const std::string& uid) override;

private:
    void apply_deadline(::grpc::ClientContext& context) const;

    std::unique_ptr<FriendRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace im::gateway

#endif  // GATEWAY_HTTP_REMOTE_FRIEND_CLIENT_HPP
