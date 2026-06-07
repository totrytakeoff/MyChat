#include "remote_group_client.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "base.pb.h"
#include "group.hpp"
#include "../../common/utils/log_manager.hpp"

namespace im::gateway {

namespace {

class GrpcGroupRpcClient final : public GroupRpcClient {
public:
    explicit GrpcGroupRpcClient(const std::string& endpoint)
        : stub_(im::group::GroupService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()))) {}

    ::grpc::Status create_group(::grpc::ClientContext* context,
                                const im::group::CreateGroupRequest& request,
                                im::group::CreateGroupResponse* response) override {
        return stub_->CreateGroup(context, request, response);
    }

    ::grpc::Status join_group(::grpc::ClientContext* context,
                              const im::group::JoinGroupRequest& request,
                              im::group::GroupActionResponse* response) override {
        return stub_->JoinGroup(context, request, response);
    }

    ::grpc::Status leave_group(::grpc::ClientContext* context,
                               const im::group::LeaveGroupRequest& request,
                               im::group::GroupActionResponse* response) override {
        return stub_->LeaveGroup(context, request, response);
    }

    ::grpc::Status group_exists(::grpc::ClientContext* context,
                                const im::group::GroupExistsRequest& request,
                                im::group::GroupExistsResponse* response) override {
        return stub_->GroupExists(context, request, response);
    }

    ::grpc::Status list_my_groups(::grpc::ClientContext* context,
                                  const im::group::GetGroupListRequest& request,
                                  im::group::GetGroupListResponse* response) override {
        return stub_->ListMyGroups(context, request, response);
    }

    ::grpc::Status list_members(::grpc::ClientContext* context,
                                const im::group::GetGroupMembersRequest& request,
                                im::group::GetGroupMembersResponse* response) override {
        return stub_->ListMembers(context, request, response);
    }

    ::grpc::Status send_message(::grpc::ClientContext* context,
                                const im::group::SendGroupMessageRequest& request,
                                im::group::SendGroupMessageResponse* response) override {
        return stub_->SendGroupMessage(context, request, response);
    }

    ::grpc::Status get_history(::grpc::ClientContext* context,
                               const im::group::GetGroupMessagesRequest& request,
                               im::group::GetGroupMessagesResponse* response) override {
        return stub_->GetGroupMessages(context, request, response);
    }

private:
    std::unique_ptr<im::group::GroupService::Stub> stub_;
};

im::service::group::GroupRole to_service_role(im::group::GroupRole role) {
    switch (role) {
    case im::group::ADMIN:
        return im::service::group::GroupRole::ADMIN;
    case im::group::OWNER:
        return im::service::group::GroupRole::OWNER;
    case im::group::MEMBER:
    default:
        return im::service::group::GroupRole::MEMBER;
    }
}

im::service::group::GroupInfoDTO to_group_info(const im::group::GroupInfo& info) {
    im::service::group::GroupInfoDTO dto;
    dto.group_id = info.group_record_id() != 0 ? info.group_record_id() : 0;
    if (dto.group_id == 0 && !info.group_id().empty()) {
        try {
            dto.group_id = std::stoull(info.group_id());
        } catch (...) {
            dto.group_id = 0;
        }
    }
    dto.name = info.name();
    dto.creator_uid = info.owner_uid();
    dto.created_at = info.create_time();
    dto.member_count = info.member_count();
    return dto;
}

im::service::group::MemberInfoDTO to_member_info(const im::group::GroupMember& info) {
    im::service::group::MemberInfoDTO dto;
    dto.id = info.member_record_id();
    dto.user_uid = info.uid();
    dto.nickname = info.nickname();
    dto.role = to_service_role(info.role());
    dto.joined_at = info.join_time();
    return dto;
}

im::service::group::GroupMessageDTO to_group_message(
    const im::group::GroupMessage& message) {
    im::service::group::GroupMessageDTO dto;
    dto.msg_id = message.msg_id();
    dto.sender_uid = message.sender_uid();
    dto.sender_nickname = message.sender_nickname();
    dto.content = message.content();
    dto.created_at = message.create_time();
    return dto;
}

std::string group_error_code(im::base::ErrorCode code) {
    switch (code) {
    case im::base::PARAM_ERROR:
        return "PARAM_ERROR";
    case im::base::NOT_FOUND:
        return "GROUP_NOT_FOUND";
    case im::base::ALREADY_EXISTS:
        return "ALREADY_MEMBER";
    case im::base::PERMISSION_DENIED:
        return "NOT_MEMBER";
    case im::base::SERVER_ERROR:
        return "REMOTE_GROUP_SERVER_ERROR";
    default:
        return "REMOTE_GROUP_ERROR";
    }
}

}  // namespace

RemoteGroupClient::RemoteGroupClient(const std::string& endpoint,
                                     std::chrono::milliseconds timeout)
    : RemoteGroupClient(std::make_unique<GrpcGroupRpcClient>(endpoint), timeout) {}

RemoteGroupClient::RemoteGroupClient(std::unique_ptr<GroupRpcClient> client,
                                     std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout) {
    im::utils::LogManager::SetLogToFile("remote_group_client",
                                        "logs/remote_group_client.log");
    logger_ = im::utils::LogManager::GetLogger("remote_group_client");
}

im::service::group::GroupResult RemoteGroupClient::create_group(
    const im::service::group::CreateGroupRequest& request) {
    im::service::group::GroupResult result;
    if (!client_) {
        result.error_code = "REMOTE_GROUP_UNAVAILABLE";
        result.message = "Remote Group client is not configured";
        return result;
    }

    im::group::CreateGroupRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(request.creator_uid);
    rpc_request.mutable_header()->set_timestamp(static_cast<uint64_t>(request.now_ms));
    rpc_request.set_name(request.name);

    im::group::CreateGroupResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->create_group(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_GROUP_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote group create RPC failed for {}: {}",
                      request.creator_uid, status.error_message());
        return result;
    }
    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = group_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }

    result.ok = true;
    result.message = "Group created";
    result.group_id = rpc_response.group_id();
    return result;
}

im::service::group::GroupResult RemoteGroupClient::join_group(
    uint64_t group_id,
    const std::string& user_uid,
    int64_t now_ms) {
    im::service::group::GroupResult result;
    if (!client_) {
        result.error_code = "REMOTE_GROUP_UNAVAILABLE";
        result.message = "Remote Group client is not configured";
        return result;
    }

    im::group::JoinGroupRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(user_uid);
    rpc_request.mutable_header()->set_timestamp(static_cast<uint64_t>(now_ms));
    rpc_request.set_group_id(group_id);

    im::group::GroupActionResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->join_group(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_GROUP_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote group join RPC failed user={} group={}: {}",
                      user_uid, group_id, status.error_message());
        return result;
    }
    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = group_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }
    result.ok = true;
    result.message = "Joined group";
    result.group_id = group_id;
    return result;
}

im::service::group::GroupResult RemoteGroupClient::leave_group(
    uint64_t group_id,
    const std::string& user_uid) {
    im::service::group::GroupResult result;
    if (!client_) {
        result.error_code = "REMOTE_GROUP_UNAVAILABLE";
        result.message = "Remote Group client is not configured";
        return result;
    }

    im::group::LeaveGroupRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(user_uid);
    rpc_request.set_group_id(group_id);

    im::group::GroupActionResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->leave_group(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_GROUP_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote group leave RPC failed user={} group={}: {}",
                      user_uid, group_id, status.error_message());
        return result;
    }
    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = group_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }
    result.ok = true;
    result.message = "Left group";
    result.group_id = group_id;
    return result;
}

std::vector<im::service::group::GroupInfoDTO> RemoteGroupClient::list_my_groups(
    const std::string& user_uid) {
    if (!client_) {
        logger_->warn("Remote group list lookup skipped: RPC client is not configured");
        return {};
    }
    im::group::GetGroupListRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(user_uid);

    im::group::GetGroupListResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);
    auto status = client_->list_my_groups(&context, rpc_request, &rpc_response);
    if (!status.ok() || rpc_response.base().error_code() != im::base::SUCCESS) {
        logger_->warn("Remote group list RPC failed for {}", user_uid);
        return {};
    }
    std::vector<im::service::group::GroupInfoDTO> groups;
    groups.reserve(static_cast<size_t>(rpc_response.groups_size()));
    for (const auto& group : rpc_response.groups()) {
        groups.push_back(to_group_info(group));
    }
    return groups;
}

bool RemoteGroupClient::group_exists(uint64_t group_id) {
    if (!client_) {
        return false;
    }
    im::group::GroupExistsRequest rpc_request;
    rpc_request.set_group_id(group_id);
    im::group::GroupExistsResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);
    auto status = client_->group_exists(&context, rpc_request, &rpc_response);
    return status.ok() &&
           rpc_response.base().error_code() == im::base::SUCCESS &&
           rpc_response.exists();
}

std::vector<im::service::group::MemberInfoDTO> RemoteGroupClient::list_members(
    uint64_t group_id,
    const std::string& caller_uid) {
    if (!client_) {
        return {};
    }
    im::group::GetGroupMembersRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(caller_uid);
    rpc_request.set_group_record_id(group_id);
    im::group::GetGroupMembersResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);
    auto status = client_->list_members(&context, rpc_request, &rpc_response);
    if (!status.ok() || rpc_response.base().error_code() != im::base::SUCCESS) {
        return {};
    }
    std::vector<im::service::group::MemberInfoDTO> members;
    members.reserve(static_cast<size_t>(rpc_response.members_size()));
    for (const auto& member : rpc_response.members()) {
        members.push_back(to_member_info(member));
    }
    return members;
}

im::service::group::GroupSendResult RemoteGroupClient::send_message(
    uint64_t group_id,
    const std::string& sender_uid,
    const std::string& content,
    int64_t now_ms) {
    im::service::group::GroupSendResult result;
    if (!client_) {
        result.error_code = "REMOTE_GROUP_UNAVAILABLE";
        result.message = "Remote Group client is not configured";
        return result;
    }
    im::group::SendGroupMessageRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(sender_uid);
    rpc_request.mutable_header()->set_timestamp(static_cast<uint64_t>(now_ms));
    rpc_request.set_group_id(group_id);
    rpc_request.set_content(content);
    rpc_request.set_create_time(now_ms);
    im::group::SendGroupMessageResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);
    auto status = client_->send_message(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_GROUP_UNAVAILABLE";
        result.message = status.error_message();
        return result;
    }
    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = group_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }
    result.ok = true;
    result.message = "Message sent";
    result.msg_id = rpc_response.message().msg_id();
    return result;
}

std::vector<im::service::group::GroupMessageDTO> RemoteGroupClient::get_history(
    uint64_t group_id,
    const std::string& caller_uid,
    int64_t before_time,
    int limit) {
    if (!client_) {
        return {};
    }
    im::group::GetGroupMessagesRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(caller_uid);
    rpc_request.set_group_record_id(group_id);
    rpc_request.set_since(before_time);
    rpc_request.set_limit(limit);
    im::group::GetGroupMessagesResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);
    auto status = client_->get_history(&context, rpc_request, &rpc_response);
    if (!status.ok() || rpc_response.base().error_code() != im::base::SUCCESS) {
        return {};
    }
    std::vector<im::service::group::GroupMessageDTO> messages;
    messages.reserve(static_cast<size_t>(rpc_response.messages_size()));
    for (const auto& message : rpc_response.messages()) {
        messages.push_back(to_group_message(message));
    }
    return messages;
}

void RemoteGroupClient::apply_deadline(::grpc::ClientContext& context) const {
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }
}

}  // namespace im::gateway
