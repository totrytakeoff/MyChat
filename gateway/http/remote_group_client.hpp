#ifndef GATEWAY_HTTP_REMOTE_GROUP_CLIENT_HPP
#define GATEWAY_HTTP_REMOTE_GROUP_CLIENT_HPP

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "group.grpc.pb.h"
#include "group_client.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class GroupRpcClient {
public:
    virtual ~GroupRpcClient() = default;

    virtual ::grpc::Status create_group(
        ::grpc::ClientContext* context,
        const im::group::CreateGroupRequest& request,
        im::group::CreateGroupResponse* response) = 0;

    virtual ::grpc::Status join_group(
        ::grpc::ClientContext* context,
        const im::group::JoinGroupRequest& request,
        im::group::GroupActionResponse* response) = 0;

    virtual ::grpc::Status leave_group(
        ::grpc::ClientContext* context,
        const im::group::LeaveGroupRequest& request,
        im::group::GroupActionResponse* response) = 0;

    virtual ::grpc::Status group_exists(
        ::grpc::ClientContext* context,
        const im::group::GroupExistsRequest& request,
        im::group::GroupExistsResponse* response) = 0;

    virtual ::grpc::Status list_my_groups(
        ::grpc::ClientContext* context,
        const im::group::GetGroupListRequest& request,
        im::group::GetGroupListResponse* response) = 0;

    virtual ::grpc::Status list_members(
        ::grpc::ClientContext* context,
        const im::group::GetGroupMembersRequest& request,
        im::group::GetGroupMembersResponse* response) = 0;

    virtual ::grpc::Status send_message(
        ::grpc::ClientContext* context,
        const im::group::SendGroupMessageRequest& request,
        im::group::SendGroupMessageResponse* response) = 0;

    virtual ::grpc::Status get_history(
        ::grpc::ClientContext* context,
        const im::group::GetGroupMessagesRequest& request,
        im::group::GetGroupMessagesResponse* response) = 0;
};

class RemoteGroupClient final : public GroupClient {
public:
    explicit RemoteGroupClient(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteGroupClient(
        std::unique_ptr<GroupRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    im::service::group::GroupResult create_group(
        const im::service::group::CreateGroupRequest& request) override;

    im::service::group::GroupResult join_group(
        uint64_t group_id,
        const std::string& user_uid,
        int64_t now_ms) override;

    im::service::group::GroupResult leave_group(
        uint64_t group_id,
        const std::string& user_uid) override;

    std::vector<im::service::group::GroupInfoDTO> list_my_groups(
        const std::string& user_uid) override;

    bool group_exists(uint64_t group_id) override;

    std::vector<im::service::group::MemberInfoDTO> list_members(
        uint64_t group_id,
        const std::string& caller_uid) override;

    im::service::group::GroupSendResult send_message(
        uint64_t group_id,
        const std::string& sender_uid,
        const std::string& content,
        int64_t now_ms) override;

    std::vector<im::service::group::GroupMessageDTO> get_history(
        uint64_t group_id,
        const std::string& caller_uid,
        int64_t before_time,
        int limit) override;

private:
    void apply_deadline(::grpc::ClientContext& context) const;

    std::unique_ptr<GroupRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace im::gateway

#endif  // GATEWAY_HTTP_REMOTE_GROUP_CLIENT_HPP
