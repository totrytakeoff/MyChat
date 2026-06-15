#ifndef IM_SERVICE_GROUP_GROUP_GRPC_SERVICE_HPP
#define IM_SERVICE_GROUP_GROUP_GRPC_SERVICE_HPP

#include <grpcpp/support/status.h>

#include "group.grpc.pb.h"

namespace grpc {
class ServerContext;
}

namespace im::service::group {

class GroupMessageService;
class GroupService;

class GroupGrpcService final : public im::group::GroupService::Service {
public:
    GroupGrpcService(GroupService* group_service,
                     GroupMessageService* group_message_service);

    ::grpc::Status CreateGroup(::grpc::ServerContext* context,
                               const im::group::CreateGroupRequest* request,
                               im::group::CreateGroupResponse* response) override;

    ::grpc::Status JoinGroup(::grpc::ServerContext* context,
                             const im::group::JoinGroupRequest* request,
                             im::group::GroupActionResponse* response) override;

    ::grpc::Status LeaveGroup(::grpc::ServerContext* context,
                              const im::group::LeaveGroupRequest* request,
                              im::group::GroupActionResponse* response) override;

    ::grpc::Status GetGroupInfo(::grpc::ServerContext* context,
                                const im::group::GetGroupInfoRequest* request,
                                im::group::GetGroupInfoResponse* response) override;

    ::grpc::Status SearchGroups(::grpc::ServerContext* context,
                                const im::group::SearchGroupsRequest* request,
                                im::group::SearchGroupsResponse* response) override;

    ::grpc::Status GroupExists(::grpc::ServerContext* context,
                               const im::group::GroupExistsRequest* request,
                               im::group::GroupExistsResponse* response) override;

    ::grpc::Status ListMyGroups(::grpc::ServerContext* context,
                                const im::group::GetGroupListRequest* request,
                                im::group::GetGroupListResponse* response) override;

    ::grpc::Status ListMembers(::grpc::ServerContext* context,
                               const im::group::GetGroupMembersRequest* request,
                               im::group::GetGroupMembersResponse* response) override;

    ::grpc::Status SendGroupMessage(::grpc::ServerContext* context,
                                    const im::group::SendGroupMessageRequest* request,
                                    im::group::SendGroupMessageResponse* response) override;

    ::grpc::Status GetGroupMessages(::grpc::ServerContext* context,
                                    const im::group::GetGroupMessagesRequest* request,
                                    im::group::GetGroupMessagesResponse* response) override;

private:
    GroupService* group_service_;
    GroupMessageService* group_message_service_;
};

}  // namespace im::service::group

#endif  // IM_SERVICE_GROUP_GROUP_GRPC_SERVICE_HPP
