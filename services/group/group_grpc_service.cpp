#include "group_grpc_service.hpp"

#include "group_message_service.hpp"
#include "group_service.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>

#include <grpcpp/server_context.h>

#include "base.pb.h"
#include "group.hpp"
#include "group.pb.h"

namespace im::service::group {

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

im::base::ErrorCode group_error_code(const std::string& error_code) {
    if (error_code == "EMPTY_NAME" ||
        error_code == "EMPTY_CREATOR" ||
        error_code == "EMPTY_UID" ||
        error_code == "EMPTY_SENDER" ||
        error_code == "INVALID_GROUP") {
        return im::base::PARAM_ERROR;
    }
    if (error_code == "GROUP_NOT_FOUND" ||
        error_code == "CREATOR_NOT_FOUND" ||
        error_code == "USER_NOT_FOUND") {
        return im::base::NOT_FOUND;
    }
    if (error_code == "ALREADY_MEMBER") {
        return im::base::ALREADY_EXISTS;
    }
    if (error_code == "NOT_MEMBER" ||
        error_code == "OWNER_CANNOT_LEAVE" ||
        error_code == "FORBIDDEN") {
        return im::base::PERMISSION_DENIED;
    }
    return im::base::SERVER_ERROR;
}

im::group::GroupRole to_proto_role(GroupRole role) {
    switch (role) {
    case GroupRole::MEMBER:
        return im::group::MEMBER;
    case GroupRole::ADMIN:
        return im::group::ADMIN;
    case GroupRole::OWNER:
        return im::group::OWNER;
    default:
        return im::group::MEMBER;
    }
}

uint64_t group_id_from(const std::string& group_id, uint64_t group_record_id) {
    if (group_record_id != 0) {
        return group_record_id;
    }
    if (group_id.empty()) {
        return 0;
    }
    try {
        return static_cast<uint64_t>(std::stoull(group_id));
    } catch (...) {
        return 0;
    }
}

void fill_group_info(const GroupInfoDTO& dto, im::group::GroupInfo* out) {
    out->set_group_id(std::to_string(dto.group_id));
    out->set_group_record_id(dto.group_id);
    out->set_name(dto.name);
    out->set_owner_uid(dto.creator_uid);
    out->set_create_time(dto.created_at);
    out->set_member_count(dto.member_count);
}

void fill_member_info(const MemberInfoDTO& dto, im::group::GroupMember* out) {
    out->set_uid(dto.user_uid);
    out->set_role(to_proto_role(dto.role));
    out->set_nickname(dto.nickname);
    out->set_join_time(dto.joined_at);
    out->set_member_record_id(dto.id);
}

void fill_group_message(uint64_t group_id,
                        const GroupMessageDTO& dto,
                        im::group::GroupMessage* out) {
    out->set_msg_id(dto.msg_id);
    out->set_group_id(group_id);
    out->set_sender_uid(dto.sender_uid);
    out->set_sender_nickname(dto.sender_nickname);
    out->set_content(dto.content);
    out->set_create_time(dto.created_at);
}

template <typename Response>
bool require_group_service(GroupService* service, Response* response) {
    if (!response) {
        return false;
    }
    if (!service) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR,
                 "GroupService is not available");
        return false;
    }
    return true;
}

template <typename Response>
bool require_group_message_service(GroupMessageService* service, Response* response) {
    if (!response) {
        return false;
    }
    if (!service) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR,
                 "GroupMessageService is not available");
        return false;
    }
    return true;
}

}  // namespace

GroupGrpcService::GroupGrpcService(GroupService* group_service,
                                   GroupMessageService* group_message_service)
    : group_service_(group_service)
    , group_message_service_(group_message_service) {}

::grpc::Status GroupGrpcService::CreateGroup(
    ::grpc::ServerContext* /*context*/,
    const im::group::CreateGroupRequest* request,
    im::group::CreateGroupResponse* response) {
    if (!require_group_service(group_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "CreateGroup request is null");
        return ::grpc::Status::OK;
    }

    try {
        CreateGroupRequest service_request;
        service_request.name = request->name();
        service_request.creator_uid = request->header().from_uid();
        service_request.now_ms = request->header().timestamp() > 0
            ? static_cast<int64_t>(request->header().timestamp())
            : now_ms();

        const auto result = group_service_->create_group(service_request);
        if (!result.ok) {
            set_base(response->mutable_base(), group_error_code(result.error_code),
                     result.message);
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        response->set_group_id(result.group_id);
        auto groups = group_service_->list_my_groups(service_request.creator_uid);
        for (const auto& group : groups) {
            if (group.group_id == result.group_id) {
                fill_group_info(group, response->mutable_group());
                break;
            }
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status GroupGrpcService::JoinGroup(
    ::grpc::ServerContext* /*context*/,
    const im::group::JoinGroupRequest* request,
    im::group::GroupActionResponse* response) {
    if (!require_group_service(group_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request || request->group_id() == 0 || request->header().from_uid().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "group_id and from_uid are required");
        return ::grpc::Status::OK;
    }

    try {
        const auto result = group_service_->join_group(
            request->group_id(),
            request->header().from_uid(),
            request->header().timestamp() > 0
                ? static_cast<int64_t>(request->header().timestamp())
                : now_ms());
        set_base(response->mutable_base(),
                 result.ok ? im::base::SUCCESS : group_error_code(result.error_code),
                 result.ok ? "" : result.message);
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status GroupGrpcService::LeaveGroup(
    ::grpc::ServerContext* /*context*/,
    const im::group::LeaveGroupRequest* request,
    im::group::GroupActionResponse* response) {
    if (!require_group_service(group_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request || request->group_id() == 0 || request->header().from_uid().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "group_id and from_uid are required");
        return ::grpc::Status::OK;
    }

    try {
        const auto result = group_service_->leave_group(
            request->group_id(), request->header().from_uid());
        set_base(response->mutable_base(),
                 result.ok ? im::base::SUCCESS : group_error_code(result.error_code),
                 result.ok ? "" : result.message);
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status GroupGrpcService::GroupExists(
    ::grpc::ServerContext* /*context*/,
    const im::group::GroupExistsRequest* request,
    im::group::GroupExistsResponse* response) {
    if (!require_group_service(group_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request || request->group_id() == 0) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "group_id is required");
        return ::grpc::Status::OK;
    }

    try {
        set_base(response->mutable_base(), im::base::SUCCESS);
        response->set_exists(group_service_->group_exists(request->group_id()));
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status GroupGrpcService::ListMyGroups(
    ::grpc::ServerContext* /*context*/,
    const im::group::GetGroupListRequest* request,
    im::group::GetGroupListResponse* response) {
    if (!require_group_service(group_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request || request->header().from_uid().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "from_uid is required");
        return ::grpc::Status::OK;
    }

    try {
        const auto groups = group_service_->list_my_groups(request->header().from_uid());
        set_base(response->mutable_base(), im::base::SUCCESS);
        for (const auto& group : groups) {
            fill_group_info(group, response->add_groups());
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status GroupGrpcService::ListMembers(
    ::grpc::ServerContext* /*context*/,
    const im::group::GetGroupMembersRequest* request,
    im::group::GetGroupMembersResponse* response) {
    if (!require_group_service(group_service_, response)) {
        return ::grpc::Status::OK;
    }
    const uint64_t group_id = request
        ? group_id_from(request->group_id(), request->group_record_id())
        : 0;
    if (!request || group_id == 0 || request->header().from_uid().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "group_id and from_uid are required");
        return ::grpc::Status::OK;
    }

    try {
        if (!group_service_->group_exists(group_id)) {
            set_base(response->mutable_base(), im::base::NOT_FOUND, "Group not found");
            return ::grpc::Status::OK;
        }

        const auto members = group_service_->list_members(
            group_id, request->header().from_uid());
        if (members.empty()) {
            set_base(response->mutable_base(), im::base::PERMISSION_DENIED,
                     "Not a member of this group");
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        for (const auto& member : members) {
            fill_member_info(member, response->add_members());
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status GroupGrpcService::SendGroupMessage(
    ::grpc::ServerContext* /*context*/,
    const im::group::SendGroupMessageRequest* request,
    im::group::SendGroupMessageResponse* response) {
    if (!require_group_service(group_service_, response) ||
        !require_group_message_service(group_message_service_, response)) {
        return ::grpc::Status::OK;
    }
    if (!request || request->group_id() == 0 ||
        request->header().from_uid().empty() || request->content().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "group_id, from_uid, and content are required");
        return ::grpc::Status::OK;
    }

    try {
        const int64_t sent_at = request->create_time() > 0
            ? request->create_time()
            : (request->header().timestamp() > 0
                ? static_cast<int64_t>(request->header().timestamp())
                : now_ms());
        const auto result = group_message_service_->send_message(
            request->group_id(), request->header().from_uid(), request->content(), sent_at);
        if (!result.ok) {
            set_base(response->mutable_base(), group_error_code(result.error_code),
                     result.message);
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        auto messages = group_message_service_->get_history(
            request->group_id(), sent_at + 1, 1);
        if (!messages.empty()) {
            fill_group_message(request->group_id(), messages.front(), response->mutable_message());
        } else {
            auto* message = response->mutable_message();
            message->set_msg_id(result.msg_id);
            message->set_group_id(request->group_id());
            message->set_sender_uid(request->header().from_uid());
            message->set_content(request->content());
            message->set_create_time(sent_at);
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status GroupGrpcService::GetGroupMessages(
    ::grpc::ServerContext* /*context*/,
    const im::group::GetGroupMessagesRequest* request,
    im::group::GetGroupMessagesResponse* response) {
    if (!require_group_service(group_service_, response) ||
        !require_group_message_service(group_message_service_, response)) {
        return ::grpc::Status::OK;
    }
    const uint64_t group_id = request
        ? group_id_from(request->group_id(), request->group_record_id())
        : 0;
    if (!request || group_id == 0 || request->header().from_uid().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "group_id and from_uid are required");
        return ::grpc::Status::OK;
    }

    try {
        if (!group_service_->group_exists(group_id)) {
            set_base(response->mutable_base(), im::base::NOT_FOUND, "Group not found");
            return ::grpc::Status::OK;
        }
        if (group_service_->list_members(group_id, request->header().from_uid()).empty()) {
            set_base(response->mutable_base(), im::base::PERMISSION_DENIED,
                     "Not a member of this group");
            return ::grpc::Status::OK;
        }

        const int limit = request->limit() > 0 ? request->limit() : 50;
        const int64_t before = request->since() > 0 ? request->since() : now_ms();
        const auto messages = group_message_service_->get_history(group_id, before, limit);
        set_base(response->mutable_base(), im::base::SUCCESS);
        for (const auto& message : messages) {
            fill_group_message(group_id, message, response->add_messages());
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

}  // namespace im::service::group
