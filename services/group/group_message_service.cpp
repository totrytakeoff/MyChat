#include "group_message_service.hpp"

#include <memory>

#include <group_message.hpp>
#include <group_message-odb.hxx>

#include <group_service.hpp>
#include <user_service.hpp>

#include "group_message_repository.hpp"

namespace im {
namespace service {
namespace group {

GroupMessageService::GroupMessageService(
    std::shared_ptr<odb::pgsql::database> db,
    std::shared_ptr<im::service::user::UserService> user_service,
    std::shared_ptr<GroupService> group_service)
    : db_(std::move(db))
    , user_service_(std::move(user_service))
    , group_service_(std::move(group_service))
    , repo_(std::make_unique<GroupMessageRepository>(db_))
{}

GroupMessageService::~GroupMessageService() = default;

GroupSendResult GroupMessageService::send_message(
    uint64_t group_id, const std::string& sender_uid,
    const std::string& content, int64_t now_ms)
{
    if (group_id == 0) {
        return {false, "INVALID_GROUP", "Invalid group id", 0};
    }
    if (sender_uid.empty()) {
        return {false, "EMPTY_SENDER", "Sender uid is empty", 0};
    }

    if (!group_service_->group_exists(group_id)) {
        return {false, "GROUP_NOT_FOUND", "Group not found", 0};
    }

    auto members = group_service_->list_members(group_id, sender_uid);
    if (members.empty()) {
        return {false, "FORBIDDEN", "Not a member of this group", 0};
    }

    GroupMessage msg(group_id, sender_uid, content, now_ms);
    if (!repo_->create(msg)) {
        return {false, "STORE_FAILED", "Failed to store message", 0};
    }

    return {true, "", "Message sent", msg.id()};
}

std::vector<GroupMessageDTO> GroupMessageService::get_history(
    uint64_t group_id, int64_t before_time, int limit)
{
    auto msgs = repo_->find_by_group(group_id, before_time, limit);

    std::vector<GroupMessageDTO> result;
    for (const auto& m : msgs) {
        GroupMessageDTO dto;
        dto.msg_id = m.id();
        dto.sender_uid = m.sender_uid();
        dto.sender_nickname = resolve_nickname(m.sender_uid());
        dto.content = m.content();
        dto.created_at = m.created_at();
        result.push_back(std::move(dto));
    }
    return result;
}

std::string GroupMessageService::resolve_nickname(const std::string& uid) {
    auto profile = user_service_->get_profile_by_uid(uid);
    if (profile.has_value()) {
        return profile->nickname;
    }
    return uid;
}

} // namespace group
} // namespace service
} // namespace im
