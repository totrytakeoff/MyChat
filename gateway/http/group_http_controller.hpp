#ifndef GATEWAY_GROUP_HTTP_CONTROLLER_HPP
#define GATEWAY_GROUP_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include <httplib.h>
#include <spdlog/logger.h>

namespace im::service::group {
class GroupService;
}

namespace im::gateway {
class MultiPlatformAuthManager;
}

namespace im {
namespace gateway {

// REST adapter for group membership workflows.
//
// The controller maps authenticated HTTP calls to GroupService and handles
// route-level status codes. Group existence, owner insertion, and membership
// authorization remain service-layer responsibilities.
class GroupHttpController {
public:
    GroupHttpController(
        std::shared_ptr<im::service::group::GroupService> group_service,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr
    );

    void handle_create_group(const httplib::Request& req, httplib::Response& res);
    void handle_join_group(const httplib::Request& req, httplib::Response& res);
    void handle_leave_group(const httplib::Request& req, httplib::Response& res);
    void handle_list_groups(const httplib::Request& req, httplib::Response& res);
    void handle_list_members(const httplib::Request& req, httplib::Response& res);

private:
    std::string extract_bearer_token(const httplib::Request& req) const;

    std::shared_ptr<im::service::group::GroupService> group_service_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace gateway
} // namespace im

#endif // GATEWAY_GROUP_HTTP_CONTROLLER_HPP
