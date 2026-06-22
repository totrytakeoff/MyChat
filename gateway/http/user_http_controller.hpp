#ifndef GATEWAY_USER_HTTP_CONTROLLER_HPP
#define GATEWAY_USER_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include "http_types.hpp"
#include <spdlog/logger.h>

namespace im::service::user {
class UserService;
}

namespace im::gateway {
class MultiPlatformAuthManager;
class UserClient;

// REST adapter for User Service account endpoints.
//
// Boundary:
// - Parses HTTP JSON and Bearer tokens.
// - Calls UserService for register/login/profile workflows.
// - Issues Gateway auth tokens through MultiPlatformAuthManager.
// - Does not expose password hashes or trust client-supplied user identity.
class UserHttpController {
public:
    UserHttpController(
        std::shared_ptr<UserClient> user_client,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr
    );

    void handle_register(const HttpRequest& req, HttpResponse& res);
    void handle_login(const HttpRequest& req, HttpResponse& res);
    void handle_profile(const HttpRequest& req, HttpResponse& res);
    void handle_update_profile(const HttpRequest& req, HttpResponse& res);
    void handle_search_user(const HttpRequest& req, HttpResponse& res);

private:
    std::string extract_bearer_token(const HttpRequest& req) const;

    std::shared_ptr<UserClient> user_client_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::gateway

#endif // GATEWAY_USER_HTTP_CONTROLLER_HPP
