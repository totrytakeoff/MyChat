#ifndef GATEWAY_GROUP_HTTP_CONTROLLER_HPP
#define GATEWAY_GROUP_HTTP_CONTROLLER_HPP

#include <memory>
#include <string>

#include "http_types.hpp"
#include <spdlog/logger.h>

namespace im::gateway {
class GroupClient;
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
        std::shared_ptr<GroupClient> group_client,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr
    );

    void handle_create_group(const HttpRequest& req, HttpResponse& res);
    void handle_join_group(const HttpRequest& req, HttpResponse& res);
    void handle_leave_group(const HttpRequest& req, HttpResponse& res);
    void handle_group_info(const HttpRequest& req, HttpResponse& res);
    void handle_search_groups(const HttpRequest& req, HttpResponse& res);
    void handle_list_groups(const HttpRequest& req, HttpResponse& res);
    void handle_list_members(const HttpRequest& req, HttpResponse& res);

private:
    std::string extract_bearer_token(const HttpRequest& req) const;

    std::shared_ptr<GroupClient> group_client_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace gateway
} // namespace im

#endif // GATEWAY_GROUP_HTTP_CONTROLLER_HPP
