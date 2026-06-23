#ifndef GATEWAY_COMMAND_HANDLER_REGISTRY_HPP
#define GATEWAY_COMMAND_HANDLER_REGISTRY_HPP

#include "../message_processor/message_processor.hpp"

#include <memory>

namespace im::gateway {

class MessageWsHandler;
class FriendServiceAdapter;
class GroupServiceAdapter;
class MessageServiceAdapter;
class UserServiceAdapter;
class MultiPlatformAuthManager;
}

namespace im::service::push {
class PushNotifier;
}

namespace im::service::user {
class UserService;
}

namespace im::service::message {
class MessageService;
}

namespace im::service::friend_ {
class FriendService;
}

namespace im::service::group {
class GroupService;
class GroupMessageService;
}

namespace im::gateway {

struct GatewayRuntimeRegistry {
    std::shared_ptr<im::service::user::UserService> user_service;
    std::shared_ptr<UserServiceAdapter> remote_user_adapter;
    std::shared_ptr<im::service::message::MessageService> message_service;
    std::shared_ptr<MessageServiceAdapter> remote_message_adapter;
    std::shared_ptr<im::service::friend_::FriendService> friend_service;
    std::shared_ptr<FriendServiceAdapter> remote_friend_adapter;
    std::shared_ptr<im::service::group::GroupService> group_service;
    std::shared_ptr<im::service::group::GroupMessageService> group_message_service;
    std::shared_ptr<GroupServiceAdapter> remote_group_adapter;
    MultiPlatformAuthManager* auth_mgr = nullptr;
    im::service::push::PushNotifier* push_notifier = nullptr;
    MessageWsHandler* message_ws = nullptr;

    im::service::user::UserService* user() const { return user_service.get(); }
    UserServiceAdapter* remote_user() const { return remote_user_adapter.get(); }
    im::service::message::MessageService* message() const { return message_service.get(); }
    MessageServiceAdapter* remote_message() const { return remote_message_adapter.get(); }
    im::service::friend_::FriendService* friend_service_ptr() const {
        return friend_service.get();
    }
    FriendServiceAdapter* remote_friend() const { return remote_friend_adapter.get(); }
    im::service::group::GroupService* group() const { return group_service.get(); }
    im::service::group::GroupMessageService* group_message() const {
        return group_message_service.get();
    }
    GroupServiceAdapter* remote_group() const { return remote_group_adapter.get(); }
};

struct GatewayCommandHandlerContext {
    const GatewayRuntimeRegistry* runtime = nullptr;
};

void register_gateway_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context);

} // namespace im::gateway

#endif // GATEWAY_COMMAND_HANDLER_REGISTRY_HPP
