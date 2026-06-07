#include "group_client.hpp"

#include <utility>

namespace im::gateway {

LocalGroupClient::LocalGroupClient(
    std::shared_ptr<im::service::group::GroupService> group_service,
    std::shared_ptr<im::service::group::GroupMessageService> group_message_service)
    : group_service_(std::move(group_service))
    , group_message_service_(std::move(group_message_service)) {}

im::service::group::GroupResult LocalGroupClient::create_group(
    const im::service::group::CreateGroupRequest& request) {
    return group_service_->create_group(request);
}

im::service::group::GroupResult LocalGroupClient::join_group(
    uint64_t group_id,
    const std::string& user_uid,
    int64_t now_ms) {
    return group_service_->join_group(group_id, user_uid, now_ms);
}

im::service::group::GroupResult LocalGroupClient::leave_group(
    uint64_t group_id,
    const std::string& user_uid) {
    return group_service_->leave_group(group_id, user_uid);
}

std::vector<im::service::group::GroupInfoDTO> LocalGroupClient::list_my_groups(
    const std::string& user_uid) {
    return group_service_->list_my_groups(user_uid);
}

bool LocalGroupClient::group_exists(uint64_t group_id) {
    return group_service_->group_exists(group_id);
}

std::vector<im::service::group::MemberInfoDTO> LocalGroupClient::list_members(
    uint64_t group_id,
    const std::string& caller_uid) {
    return group_service_->list_members(group_id, caller_uid);
}

im::service::group::GroupSendResult LocalGroupClient::send_message(
    uint64_t group_id,
    const std::string& sender_uid,
    const std::string& content,
    int64_t now_ms) {
    return group_message_service_->send_message(group_id, sender_uid, content, now_ms);
}

std::vector<im::service::group::GroupMessageDTO> LocalGroupClient::get_history(
    uint64_t group_id,
    const std::string& /*caller_uid*/,
    int64_t before_time,
    int limit) {
    return group_message_service_->get_history(group_id, before_time, limit);
}

}  // namespace im::gateway
