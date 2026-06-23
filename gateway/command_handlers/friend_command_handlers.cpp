#include "gateway_command_handler_modules.hpp"

#include <exception>
#include <string>

#include "../../common/proto/command.pb.h"
#include "../../common/utils/log_manager.hpp"
#include "../../services/friend/friend_packet_dispatcher.hpp"
#include "../../services/friend/friend_service.hpp"
#include "../service_adapters/friend_service_adapter.hpp"
#include "command_handler_utils.hpp"

namespace im::gateway {
namespace {

using namespace im::gateway::command_handlers;
using im::utils::LogManager;

ProcessorResult handle_friend_packet_http(const UnifiedMessage& msg,
                                          im::service::friend_::FriendService* friend_service,
                                          FriendServiceAdapter* remote_friend_adapter,
                                          MultiPlatformAuthManager* auth_mgr) {
    auto ready = require_http_dependency(msg, friend_service || remote_friend_adapter,
                                     "Friend service is not initialized");
    if (ready.status_code != 0 || ready.http_status != 0) {
        return ready;
    }
    UserTokenInfo user_info;
    auto auth = verify_http_user(msg, auth_mgr, user_info);
    if (auth.status_code != 0 || auth.http_status != 0) {
        return auth;
    }

    try {
        im::friend_::FriendPacketRequest packet;
        packet.mutable_header()->CopyFrom(msg.get_header());
        packet.mutable_header()->set_from_uid(user_info.user_id);
        packet.mutable_header()->set_device_id(user_info.device_id);
        packet.mutable_header()->set_platform(user_info.platform);
        packet.set_type_name("application/json");
        packet.set_payload(msg.get_session_context().raw_body.empty()
                ? msg.get_json_body()
                : msg.get_session_context().raw_body);

        im::friend_::FriendPacketResponse result;
        if (remote_friend_adapter) {
            result = remote_friend_adapter->forward(packet);
        } else {
            im::service::friend_::FriendPacketDispatcher dispatcher(friend_service);
            result = dispatcher.handle(packet);
        }

        const bool ok = result.base().error_code() == im::base::SUCCESS;
        const int http_status = result.http_status() != 0
                ? result.http_status()
                : (ok ? 200 : http_status_from_error(result.base().error_code()));
        return ProcessorResult::http(http_status, result.payload());
    } catch (const std::exception& e) {
        LogManager::GetLogger("gateway_command_handlers")
                ->error("handle_friend_packet_http exception: {}", e.what());
        return error_response(500, "Internal server error");
    }
}

} // namespace

void register_friend_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context) {
    const auto* runtime = context.runtime;
    auto* friend_service = runtime ? runtime->friend_service_ptr() : nullptr;
    auto* remote_friend_adapter = runtime ? runtime->remote_friend() : nullptr;
    auto* auth_mgr = runtime ? runtime->auth_mgr : nullptr;

    register_handler(processor, im::command::CMD_ADD_FRIEND,
        [friend_service, remote_friend_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_friend_packet_http(msg, friend_service, remote_friend_adapter, auth_mgr);
        });
    register_handler(processor, im::command::CMD_HANDLE_FRIEND_REQUEST,
        [friend_service, remote_friend_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_friend_packet_http(msg, friend_service, remote_friend_adapter, auth_mgr);
        });
    register_handler(processor, im::command::CMD_GET_FRIEND_LIST,
        [friend_service, remote_friend_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_friend_packet_http(msg, friend_service, remote_friend_adapter, auth_mgr);
        });
    register_handler(processor, im::command::CMD_GET_FRIEND_REQUESTS,
        [friend_service, remote_friend_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_friend_packet_http(msg, friend_service, remote_friend_adapter, auth_mgr);
        });
}

} // namespace im::gateway
