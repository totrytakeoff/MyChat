#ifndef GATEWAY_USER_HTTP_CONTROLLER_HPP
#define GATEWAY_USER_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include <httplib.h>
#include <spdlog/logger.h>

namespace im::service::user {
class UserService;
}

namespace im::gateway {
class MultiPlatformAuthManager;

class UserHttpController {
public:
    UserHttpController(
        std::shared_ptr<im::service::user::UserService> user_service,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr
    );

    void handle_register(const httplib::Request& req, httplib::Response& res);
    void handle_login(const httplib::Request& req, httplib::Response& res);
    void handle_profile(const httplib::Request& req, httplib::Response& res);

private:
    std::string extract_bearer_token(const httplib::Request& req) const;

    std::shared_ptr<im::service::user::UserService> user_service_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::gateway

#endif // GATEWAY_USER_HTTP_CONTROLLER_HPP
