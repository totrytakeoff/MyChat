#ifndef IM_SERVICE_GROUP_GROUP_PACKET_DISPATCHER_HPP
#define IM_SERVICE_GROUP_GROUP_PACKET_DISPATCHER_HPP

#include "group.pb.h"

namespace im::service::group {

class GroupMessageService;
class GroupService;

class GroupPacketDispatcher {
public:
    GroupPacketDispatcher(GroupService* group_service,
                          GroupMessageService* group_message_service);

    im::group::GroupPacketResponse handle(const im::group::GroupPacketRequest& packet);

private:
    im::group::GroupPacketResponse handle_create_group(const im::group::GroupPacketRequest& packet);
    im::group::GroupPacketResponse handle_search_group(const im::group::GroupPacketRequest& packet);
    im::group::GroupPacketResponse handle_group_info(const im::group::GroupPacketRequest& packet);
    im::group::GroupPacketResponse handle_group_list(const im::group::GroupPacketRequest& packet);
    im::group::GroupPacketResponse handle_join_group(const im::group::GroupPacketRequest& packet);
    im::group::GroupPacketResponse handle_leave_group(const im::group::GroupPacketRequest& packet);
    im::group::GroupPacketResponse handle_group_members(const im::group::GroupPacketRequest& packet);
    im::group::GroupPacketResponse handle_send_group_message(const im::group::GroupPacketRequest& packet);
    im::group::GroupPacketResponse handle_group_history(const im::group::GroupPacketRequest& packet);

    GroupService* group_service_;
    GroupMessageService* group_message_service_;
};

}  // namespace im::service::group

#endif  // IM_SERVICE_GROUP_GROUP_PACKET_DISPATCHER_HPP
