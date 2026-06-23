#ifndef IM_SERVICE_FRIEND_FRIEND_GRPC_SERVICE_HPP
#define IM_SERVICE_FRIEND_FRIEND_GRPC_SERVICE_HPP

#include <grpcpp/support/status.h>

#include "friend.grpc.pb.h"

namespace grpc {
class ServerContext;
}

namespace im::service::friend_ {

class FriendService;

class FriendGrpcService final : public im::friend_::FriendService::Service {
public:
    explicit FriendGrpcService(FriendService* friend_service);

    ::grpc::Status ForwardPacket(::grpc::ServerContext* context,
                                 const im::friend_::FriendPacketRequest* request,
                                 im::friend_::FriendPacketResponse* response) override;

    ::grpc::Status SendRequest(::grpc::ServerContext* context,
                               const im::friend_::AddFriendRequest* request,
                               im::friend_::AddFriendResponse* response) override;

    ::grpc::Status RespondToRequest(::grpc::ServerContext* context,
                                    const im::friend_::HandleFriendRequest* request,
                                    im::friend_::HandleFriendResponse* response) override;

    ::grpc::Status GetFriends(::grpc::ServerContext* context,
                              const im::friend_::GetFriendListRequest* request,
                              im::friend_::GetFriendListResponse* response) override;

    ::grpc::Status GetPendingRequests(
        ::grpc::ServerContext* context,
        const im::friend_::GetFriendRequestsRequest* request,
        im::friend_::GetFriendRequestsResponse* response) override;

private:
    FriendService* friend_service_;
};

}  // namespace im::service::friend_

#endif  // IM_SERVICE_FRIEND_FRIEND_GRPC_SERVICE_HPP
