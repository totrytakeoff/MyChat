#ifndef GATEWAY_COMMAND_HANDLER_UTILS_HPP
#define GATEWAY_COMMAND_HANDLER_UTILS_HPP

#include <cstdint>
#include <functional>
#include <string>

#include "../../common/proto/base.pb.h"
#include "../auth/multi_platform_auth.hpp"
#include "../message_processor/message_processor.hpp"

namespace im::gateway::command_handlers {

int64_t now_ms();

ProcessorResult error_response(int status, std::string message);

bool register_handler(
        MessageProcessor& processor,
        uint32_t cmd_id,
        std::function<ProcessorResult(const UnifiedMessage&)> handler);

int http_status_from_error(im::base::ErrorCode code);

inline ProcessorResult require_http_dependency(const UnifiedMessage& msg,
                                               bool ready,
                                               const char* missing_message) {
    if (!msg.is_http()) {
        return ProcessorResult(im::base::ErrorCode::INVALID_REQUEST,
                               "HTTP handler called for non-HTTP message");
    }
    if (!ready) {
        return ProcessorResult(im::base::ErrorCode::SERVER_ERROR, missing_message);
    }
    return ProcessorResult();
}

ProcessorResult verify_http_user(const UnifiedMessage& msg,
                                 MultiPlatformAuthManager* auth_mgr,
                                 UserTokenInfo& user_info);

} // namespace im::gateway::command_handlers

#endif // GATEWAY_COMMAND_HANDLER_UTILS_HPP
