#ifndef GATEWAY_MESSAGE_HTTP_CONTROLLER_HPP
#define GATEWAY_MESSAGE_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include <httplib.h>
#include <spdlog/logger.h>

namespace im::gateway {
class MultiPlatformAuthManager;
class MessageClient;
}

namespace im::service::push {
class PushNotifier;
}

namespace im::gateway {

// REST adapter for one-to-one message workflows.
//
// The verified access token is the actor identity for send/history/offline
// operations. Client request bodies may name peers and content, but they do not
// decide the sender UID.
class MessageHttpController {
public:
    MessageHttpController(
        std::shared_ptr<MessageClient> msg_client,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
        im::service::push::PushNotifier* push_notifier = nullptr
    );

    void set_push_notifier(im::service::push::PushNotifier* notifier) {
        push_notifier_ = notifier;
    }

    void handle_send(const httplib::Request& req, httplib::Response& res);
    void handle_history(const httplib::Request& req, httplib::Response& res);
    void handle_offline(const httplib::Request& req, httplib::Response& res);

private:
    std::string extract_bearer_token(const httplib::Request& req) const;

    std::shared_ptr<MessageClient> msg_client_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    im::service::push::PushNotifier* push_notifier_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::gateway

#endif // GATEWAY_MESSAGE_HTTP_CONTROLLER_HPP
