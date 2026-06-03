#include "group_service.hpp"
#include "group_repository.hpp"

#include <utility>
#include <chrono>

#include <odb/pgsql/database.hxx>

#include <group.hpp>
#include "../user/user_service.hpp"

namespace im {
namespace service {
namespace group {

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // anonymous namespace

GroupService::GroupService(
    std::shared_ptr<odb::pgsql::database> db,
    std::shared_ptr<im::service::user::UserService> user_service)
    : db_(std::move(db))
    , user_service_(std::move(user_service))
    , repo_(std::make_unique<GroupRepository>(db_)) {}

GroupService::~GroupService() = default;

GroupResult GroupService::create_group(const CreateGroupRequest& request) {
    GroupResult result;

    if (request.name.empty()) {
        result.error_code = "EMPTY_NAME";
        result.message = "Group name must not be empty";
        return result;
    }

    if (request.creator_uid.empty()) {
        result.error_code = "EMPTY_CREATOR";
        result.message = "Creator UID must not be empty";
        return result;
    }

    if (!user_service_->get_profile_by_uid(request.creator_uid).has_value()) {
        result.error_code = "CREATOR_NOT_FOUND";
        result.message = "Creator user does not exist";
        return result;
    }

    int64_t t = request.now_ms > 0 ? request.now_ms : now_ms();

    Group g(request.name, request.creator_uid, t);
    GroupMember owner(0, request.creator_uid, GroupRole::OWNER, t);
    if (!repo_->create_group_with_owner(g, owner)) {
        result.error_code = "PERSIST_FAILED";
        result.message = "Failed to persist group with owner";
        return result;
    }

    result.ok = true;
    result.group_id = g.group_id();
    result.message = "Group created";
    return result;
}

GroupResult GroupService::join_group(uint64_t group_id,
                                     const std::string& user_uid,
                                     int64_t t) {
    GroupResult result;

    auto group = repo_->find_group_by_id(group_id);
    if (!group.has_value()) {
        result.error_code = "GROUP_NOT_FOUND";
        result.message = "Group not found";
        return result;
    }

    if (user_uid.empty()) {
        result.error_code = "EMPTY_UID";
        result.message = "User UID must not be empty";
        return result;
    }

    if (!user_service_->get_profile_by_uid(user_uid).has_value()) {
        result.error_code = "USER_NOT_FOUND";
        result.message = "User does not exist";
        return result;
    }

    auto existing = repo_->find_member(group_id, user_uid);
    if (existing.has_value()) {
        result.error_code = "ALREADY_MEMBER";
        result.message = "User is already a member of this group";
        return result;
    }

    int64_t joined_at = t > 0 ? t : now_ms();

    GroupMember m(group_id, user_uid, GroupRole::MEMBER, joined_at);
    if (!repo_->add_member(m)) {
        result.error_code = "PERSIST_FAILED";
        result.message = "Failed to add member";
        return result;
    }

    result.ok = true;
    result.message = "Joined group";
    return result;
}

GroupResult GroupService::leave_group(uint64_t group_id,
                                      const std::string& user_uid) {
    GroupResult result;

    auto group = repo_->find_group_by_id(group_id);
    if (!group.has_value()) {
        result.error_code = "GROUP_NOT_FOUND";
        result.message = "Group not found";
        return result;
    }

    auto member = repo_->find_member(group_id, user_uid);
    if (!member.has_value()) {
        result.error_code = "NOT_MEMBER";
        result.message = "User is not a member of this group";
        return result;
    }

    // Owner cannot leave (must transfer ownership first)
    if (member->role() == GroupRole::OWNER) {
        result.error_code = "OWNER_CANNOT_LEAVE";
        result.message = "Owner cannot leave the group; transfer ownership first";
        return result;
    }

    if (!repo_->remove_member(group_id, user_uid)) {
        result.error_code = "REMOVE_FAILED";
        result.message = "Failed to remove member";
        return result;
    }

    result.ok = true;
    result.message = "Left group";
    return result;
}

std::vector<GroupInfoDTO> GroupService::list_my_groups(
    const std::string& user_uid)
{
    std::vector<GroupInfoDTO> result;
    auto groups = repo_->find_groups_by_user(user_uid);
    result.reserve(groups.size());
    for (const auto& g : groups) {
        GroupInfoDTO dto;
        dto.group_id = g.group_id();
        dto.name = g.name();
        dto.creator_uid = g.creator_uid();
        dto.created_at = g.created_at();
        dto.member_count = static_cast<int>(repo_->find_members(g.group_id()).size());
        result.push_back(dto);
    }
    return result;
}

bool GroupService::group_exists(uint64_t group_id) {
    return repo_->find_group_by_id(group_id).has_value();
}

std::vector<MemberInfoDTO> GroupService::list_members(
    uint64_t group_id, const std::string& caller_uid)
{
    std::vector<MemberInfoDTO> result;

    auto group = repo_->find_group_by_id(group_id);
    if (!group.has_value()) {
        return result;
    }

    // Only members can list members
    if (!repo_->find_member(group_id, caller_uid).has_value()) {
        return result;
    }

    auto members = repo_->find_members(group_id);
    result.reserve(members.size());
    for (const auto& m : members) {
        MemberInfoDTO dto;
        dto.id = m.id();
        dto.user_uid = m.user_uid();
        dto.nickname = resolve_nickname(m.user_uid());
        dto.role = m.role();
        dto.joined_at = m.joined_at();
        result.push_back(dto);
    }
    return result;
}

std::string GroupService::resolve_nickname(const std::string& uid) {
    if (auto profile = user_service_->get_profile_by_uid(uid)) {
        return profile->nickname;
    }
    return uid;
}

} // namespace group
} // namespace service
} // namespace im
