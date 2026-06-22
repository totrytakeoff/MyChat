#ifndef GATEWAY_FRIEND_HTTP_CONTROLLER_HPP
#define GATEWAY_FRIEND_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include "http_types.hpp"
#include <spdlog/logger.h>

namespace im::gateway {
class FriendClient;
class MultiPlatformAuthManager;
}

namespace im {
namespace gateway {

// REST adapter for Friend Service request/list workflows.
//
// This controller performs HTTP/auth translation only. Pair uniqueness,
// pending/accepted state transitions, and friend-list semantics stay in
// FriendService.
class FriendHttpController {
public:
    FriendHttpController(
        std::shared_ptr<FriendClient> friend_client,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr
    );

    void handle_send_request(const HttpRequest& req, HttpResponse& res);
    void handle_respond_request(const HttpRequest& req, HttpResponse& res);
    void handle_list_friends(const HttpRequest& req, HttpResponse& res);
    void handle_pending_requests(const HttpRequest& req, HttpResponse& res);

private:
    std::string extract_bearer_token(const HttpRequest& req) const;

    std::shared_ptr<FriendClient> friend_client_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace gateway
} // namespace im

#endif // GATEWAY_FRIEND_HTTP_CONTROLLER_HPP
