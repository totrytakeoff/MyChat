#ifndef GATEWAY_FRIEND_HTTP_CONTROLLER_HPP
#define GATEWAY_FRIEND_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include <httplib.h>
#include <spdlog/logger.h>

namespace im::service::friend_ {
class FriendService;
}

namespace im::gateway {
class MultiPlatformAuthManager;
}

namespace im {
namespace gateway {

class FriendHttpController {
public:
    FriendHttpController(
        std::shared_ptr<im::service::friend_::FriendService> friend_service,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr
    );

    void handle_send_request(const httplib::Request& req, httplib::Response& res);
    void handle_respond_request(const httplib::Request& req, httplib::Response& res);
    void handle_list_friends(const httplib::Request& req, httplib::Response& res);
    void handle_pending_requests(const httplib::Request& req, httplib::Response& res);

private:
    std::string extract_bearer_token(const httplib::Request& req) const;

    std::shared_ptr<im::service::friend_::FriendService> friend_service_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace gateway
} // namespace im

#endif // GATEWAY_FRIEND_HTTP_CONTROLLER_HPP
