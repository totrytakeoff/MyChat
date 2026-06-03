#ifndef GATEWAY_GROUP_MESSAGE_HTTP_CONTROLLER_HPP
#define GATEWAY_GROUP_MESSAGE_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include <httplib.h>
#include <spdlog/logger.h>

namespace im::service::group {
class GroupService;
class GroupMessageService;
}

namespace im::gateway {
class MultiPlatformAuthManager;
class PushService;
}

namespace im {
namespace gateway {

class GroupMessageHttpController {
public:
    GroupMessageHttpController(
        std::shared_ptr<im::service::group::GroupService> group_service,
        std::shared_ptr<im::service::group::GroupMessageService> group_msg_service,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
        PushService* push_service = nullptr
    );

    void set_push_service(PushService* ps) { push_service_ = ps; }

    void handle_send_message(const httplib::Request& req, httplib::Response& res);
    void handle_get_history(const httplib::Request& req, httplib::Response& res);

private:
    std::string extract_bearer_token(const httplib::Request& req) const;

    std::shared_ptr<im::service::group::GroupService> group_service_;
    std::shared_ptr<im::service::group::GroupMessageService> group_msg_service_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    PushService* push_service_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace gateway
} // namespace im

#endif // GATEWAY_GROUP_MESSAGE_HTTP_CONTROLLER_HPP
