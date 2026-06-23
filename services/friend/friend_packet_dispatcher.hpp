#ifndef IM_SERVICE_FRIEND_FRIEND_PACKET_DISPATCHER_HPP
#define IM_SERVICE_FRIEND_FRIEND_PACKET_DISPATCHER_HPP

#include "friend.pb.h"

namespace im::service::friend_ {

class FriendService;

class FriendPacketDispatcher {
public:
    explicit FriendPacketDispatcher(FriendService* friend_service);

    im::friend_::FriendPacketResponse handle(const im::friend_::FriendPacketRequest& packet);

private:
    im::friend_::FriendPacketResponse handle_add_friend(const im::friend_::FriendPacketRequest& packet);
    im::friend_::FriendPacketResponse handle_friend_response(const im::friend_::FriendPacketRequest& packet);
    im::friend_::FriendPacketResponse handle_friend_list(const im::friend_::FriendPacketRequest& packet);
    im::friend_::FriendPacketResponse handle_pending_friends(const im::friend_::FriendPacketRequest& packet);

    FriendService* friend_service_;
};

}  // namespace im::service::friend_

#endif  // IM_SERVICE_FRIEND_FRIEND_PACKET_DISPATCHER_HPP
