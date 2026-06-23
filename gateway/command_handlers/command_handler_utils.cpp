#include "command_handler_utils.hpp"

#include <chrono>
#include <utility>

#include "../../common/utils/http_utils.hpp"

namespace im::gateway::command_handlers {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
}

ProcessorResult error_response(int status, std::string message) {
    return ProcessorResult::http(
            status,
            im::utils::HttpUtils::buildResponseString(status, "", std::move(message)));
}

bool register_handler(
        MessageProcessor& processor,
        uint32_t cmd_id,
        std::function<ProcessorResult(const UnifiedMessage&)> handler) {
    const int ret = processor.register_processor(cmd_id, std::move(handler));
    return ret == 0;
}

int http_status_from_error(im::base::ErrorCode code) {
    switch (code) {
        case im::base::AUTH_FAILED:
            return 401;
        case im::base::INVALID_REQUEST:
        case im::base::PARAM_ERROR:
            return 400;
        case im::base::NOT_FOUND:
            return 404;
        case im::base::PERMISSION_DENIED:
            return 403;
        case im::base::ALREADY_EXISTS:
            return 409;
        default:
            return 500;
    }
}

ProcessorResult verify_http_user(const UnifiedMessage& msg,
                                 MultiPlatformAuthManager* auth_mgr,
                                 UserTokenInfo& user_info) {
    if (!auth_mgr) {
        return ProcessorResult(im::base::ErrorCode::SERVER_ERROR,
                               "Auth manager is not initialized");
    }
    if (msg.get_token().empty()) {
        return error_response(401, "Missing or invalid Authorization header");
    }
    if (!auth_mgr->verify_access_token(msg.get_token(), user_info)) {
        return error_response(401, "Invalid or expired access token");
    }
    return ProcessorResult();
}

} // namespace im::gateway::command_handlers
