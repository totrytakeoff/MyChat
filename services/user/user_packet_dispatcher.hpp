#ifndef IM_SERVICE_USER_USER_PACKET_DISPATCHER_HPP
#define IM_SERVICE_USER_USER_PACKET_DISPATCHER_HPP

#include "user.pb.h"

namespace im::service::user {

class UserService;

class UserPacketDispatcher {
public:
    explicit UserPacketDispatcher(UserService* user_service);

    im::user::UserPacketResponse handle(const im::user::UserPacketRequest& packet);

private:
    im::user::UserPacketResponse handle_register(const im::user::UserPacketRequest& packet);
    im::user::UserPacketResponse handle_login(const im::user::UserPacketRequest& packet);
    im::user::UserPacketResponse handle_profile(const im::user::UserPacketRequest& packet);
    im::user::UserPacketResponse handle_update_profile(const im::user::UserPacketRequest& packet);
    im::user::UserPacketResponse handle_search_user(const im::user::UserPacketRequest& packet);

    UserService* user_service_;
};

}  // namespace im::service::user

#endif  // IM_SERVICE_USER_USER_PACKET_DISPATCHER_HPP
