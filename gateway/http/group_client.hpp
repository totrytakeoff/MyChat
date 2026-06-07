#ifndef GATEWAY_HTTP_GROUP_CLIENT_HPP
#define GATEWAY_HTTP_GROUP_CLIENT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../services/group/group_message_service.hpp"
#include "../../services/group/group_service.hpp"

namespace im::gateway {

class GroupClient {
public:
    virtual ~GroupClient() = default;

    virtual im::service::group::GroupResult create_group(
        const im::service::group::CreateGroupRequest& request) = 0;

    virtual im::service::group::GroupResult join_group(
        uint64_t group_id,
        const std::string& user_uid,
        int64_t now_ms) = 0;

    virtual im::service::group::GroupResult leave_group(
        uint64_t group_id,
        const std::string& user_uid) = 0;

    virtual std::vector<im::service::group::GroupInfoDTO> list_my_groups(
        const std::string& user_uid) = 0;

    virtual bool group_exists(uint64_t group_id) = 0;

    virtual std::vector<im::service::group::MemberInfoDTO> list_members(
        uint64_t group_id,
        const std::string& caller_uid) = 0;

    virtual im::service::group::GroupSendResult send_message(
        uint64_t group_id,
        const std::string& sender_uid,
        const std::string& content,
        int64_t now_ms) = 0;

    virtual std::vector<im::service::group::GroupMessageDTO> get_history(
        uint64_t group_id,
        const std::string& caller_uid,
        int64_t before_time,
        int limit) = 0;
};

class LocalGroupClient final : public GroupClient {
public:
    LocalGroupClient(std::shared_ptr<im::service::group::GroupService> group_service,
                     std::shared_ptr<im::service::group::GroupMessageService> group_message_service);

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
    std::shared_ptr<im::service::group::GroupService> group_service_;
    std::shared_ptr<im::service::group::GroupMessageService> group_message_service_;
};

}  // namespace im::gateway

#endif  // GATEWAY_HTTP_GROUP_CLIENT_HPP
