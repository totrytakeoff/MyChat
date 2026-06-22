#ifndef GATEWAY_GROUP_MESSAGE_HTTP_CONTROLLER_HPP
#define GATEWAY_GROUP_MESSAGE_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include "http_types.hpp"
#include <spdlog/logger.h>

namespace im::gateway {
class GroupClient;
class MultiPlatformAuthManager;
}

namespace im::service::push {
class PushNotifier;
}

namespace im {
namespace gateway {

// REST adapter for group message send/history workflows.
//
// GroupMessageService validates persistence and sender membership. When a send
// succeeds, this controller may use PushNotifier to fan out the persisted
// message to other online group members.
class GroupMessageHttpController {
public:
    GroupMessageHttpController(
        std::shared_ptr<GroupClient> group_client,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
        im::service::push::PushNotifier* push_notifier = nullptr
    );

    void set_push_notifier(im::service::push::PushNotifier* notifier) {
        push_notifier_ = notifier;
    }

    void handle_send_message(const HttpRequest& req, HttpResponse& res);
    void handle_get_history(const HttpRequest& req, HttpResponse& res);

private:
    std::string extract_bearer_token(const HttpRequest& req) const;

    std::shared_ptr<GroupClient> group_client_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    im::service::push::PushNotifier* push_notifier_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace gateway
} // namespace im

#endif // GATEWAY_GROUP_MESSAGE_HTTP_CONTROLLER_HPP
