#include "gateway_command_handler_modules.hpp"

#include <exception>
#include <string>

#include <nlohmann/json.hpp>

#include "../../common/proto/command.pb.h"
#include "../../common/utils/log_manager.hpp"
#include "../../services/user/user_packet_dispatcher.hpp"
#include "../../services/user/user_service.hpp"
#include "../service_adapters/user_service_adapter.hpp"
#include "command_handler_utils.hpp"

namespace im::gateway {
namespace {

using namespace im::gateway::command_handlers;
using im::utils::LogManager;

bool command_requires_auth(uint32_t cmd_id) {
    return cmd_id != im::command::CMD_REGISTER &&
           cmd_id != im::command::CMD_LOGIN;
}

im::user::UserPacketRequest make_user_packet(const UnifiedMessage& msg) {
    im::user::UserPacketRequest packet;
    packet.mutable_header()->CopyFrom(msg.get_header());
    packet.set_type_name("application/json");
    packet.set_payload(msg.get_session_context().raw_body.empty()
            ? msg.get_json_body()
            : msg.get_session_context().raw_body);
    return packet;
}

ProcessorResult append_tokens_and_respond(im::user::UserPacketResponse result,
                                          MultiPlatformAuthManager* auth_mgr) {
    const bool ok = result.base().error_code() == im::base::SUCCESS;
    if (ok && result.auth_token_hint().required()) {
        if (!auth_mgr) {
            return ProcessorResult(im::base::SERVER_ERROR,
                                   "Auth manager is not initialized");
        }
        if (result.auth_token_hint().uid().empty() ||
            result.auth_token_hint().account().empty()) {
            return error_response(500, "User service returned incomplete auth token hint");
        }

        auto token_result = auth_mgr->generate_tokens(
                result.auth_token_hint().uid(),
                result.auth_token_hint().account(),
                result.auth_token_hint().device_id().empty()
                        ? "http-default"
                        : result.auth_token_hint().device_id(),
                result.auth_token_hint().platform().empty()
                        ? "web"
                        : result.auth_token_hint().platform());
        if (!token_result.success) {
            return error_response(500, token_result.error_message.empty()
                    ? "Failed to generate auth tokens"
                    : token_result.error_message);
        }

        try {
            auto body = nlohmann::json::parse(result.payload());
            body["access_token"] = token_result.new_access_token;
            body["refresh_token"] = token_result.new_refresh_token;
            result.set_payload(body.dump());
        } catch (const nlohmann::json::exception&) {
            return error_response(500, "Invalid user auth response payload");
        }
    }

    const int http_status = result.http_status() != 0
            ? result.http_status()
            : (ok ? 200 : http_status_from_error(result.base().error_code()));
    return ProcessorResult::http(http_status, result.payload());
}

ProcessorResult handle_user_packet_http(const UnifiedMessage& msg,
                                        im::service::user::UserService* user_service,
                                        UserServiceAdapter* remote_user_adapter,
                                        MultiPlatformAuthManager* auth_mgr) {
    auto ready = require_http_dependency(msg, user_service || remote_user_adapter,
                                     "User service is not initialized");
    if (ready.status_code != 0 || ready.http_status != 0) {
        return ready;
    }

    UserTokenInfo user_info;
    if (command_requires_auth(msg.get_cmd_id())) {
        auto auth = verify_http_user(msg, auth_mgr, user_info);
        if (auth.status_code != 0 || auth.http_status != 0) {
            return auth;
        }
    }

    try {
        im::user::UserPacketRequest packet = make_user_packet(msg);
        if (command_requires_auth(msg.get_cmd_id())) {
            packet.mutable_header()->set_from_uid(user_info.user_id);
            packet.mutable_header()->set_device_id(user_info.device_id);
            packet.mutable_header()->set_platform(user_info.platform);
        }

        im::user::UserPacketResponse result;
        if (remote_user_adapter) {
            result = remote_user_adapter->forward(packet);
        } else {
            im::service::user::UserPacketDispatcher dispatcher(user_service);
            result = dispatcher.handle(packet);
        }

        return append_tokens_and_respond(std::move(result), auth_mgr);
    } catch (const std::exception& e) {
        LogManager::GetLogger("gateway_command_handlers")
                ->error("handle_user_packet_http exception: {}", e.what());
        return error_response(500, "Internal server error");
    }
}

} // namespace

void register_user_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context) {
    const auto* runtime = context.runtime;
    auto* user_service = runtime ? runtime->user() : nullptr;
    auto* remote_user_adapter = runtime ? runtime->remote_user() : nullptr;
    auto* auth_mgr = runtime ? runtime->auth_mgr : nullptr;

    register_handler(processor, im::command::CMD_REGISTER,
        [user_service, remote_user_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_user_packet_http(msg, user_service, remote_user_adapter, auth_mgr);
        });
    register_handler(processor, im::command::CMD_LOGIN,
        [user_service, remote_user_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_user_packet_http(msg, user_service, remote_user_adapter, auth_mgr);
        });
    register_handler(processor, im::command::CMD_GET_USER_INFO,
        [user_service, remote_user_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_user_packet_http(msg, user_service, remote_user_adapter, auth_mgr);
        });
    register_handler(processor, im::command::CMD_UPDATE_USER_INFO,
        [user_service, remote_user_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_user_packet_http(msg, user_service, remote_user_adapter, auth_mgr);
        });
    register_handler(processor, im::command::CMD_SEARCH_USER,
        [user_service, remote_user_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_user_packet_http(msg, user_service, remote_user_adapter, auth_mgr);
        });
}

} // namespace im::gateway
