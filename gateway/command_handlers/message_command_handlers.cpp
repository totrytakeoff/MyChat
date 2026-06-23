#include "gateway_command_handler_modules.hpp"

#include <exception>
#include <string>

#include "../../common/proto/command.pb.h"
#include "../../common/utils/log_manager.hpp"
#include "../../services/message/message_packet_dispatcher.hpp"
#include "../../services/message/message_service.hpp"
#include "../../services/push/push_notifier.hpp"
#include "../service_adapters/message_service_adapter.hpp"
#include "command_handler_utils.hpp"

#ifdef IM_ENABLE_MESSAGE_WS
#include "../ws/message_ws_handler.hpp"
#endif

namespace im::gateway {
namespace {

using namespace im::gateway::command_handlers;
using im::utils::LogManager;

ProcessorResult handle_send_message_http(const UnifiedMessage& msg,
                                         im::service::message::MessageService* message_service,
                                         MessageServiceAdapter* remote_message_adapter,
                                         MultiPlatformAuthManager* auth_mgr,
                                         im::service::push::PushNotifier* push_notifier) {
    auto ready = require_http_dependency(msg, message_service || remote_message_adapter,
                                     "Message service is not initialized");
    if (ready.status_code != 0 || ready.http_status != 0) {
        return ready;
    }
    UserTokenInfo user_info;
    auto auth = verify_http_user(msg, auth_mgr, user_info);
    if (auth.status_code != 0 || auth.http_status != 0) {
        return auth;
    }

    try {
        im::message::MessagePacketRequest packet;
        packet.mutable_header()->CopyFrom(msg.get_header());
        packet.mutable_header()->set_from_uid(user_info.user_id);
        packet.mutable_header()->set_device_id(user_info.device_id);
        packet.mutable_header()->set_platform(user_info.platform);
        packet.set_type_name("application/json");
        packet.set_payload(msg.get_session_context().raw_body.empty()
                ? msg.get_json_body()
                : msg.get_session_context().raw_body);

        im::message::MessagePacketResponse result;
        if (remote_message_adapter) {
            result = remote_message_adapter->forward(packet);
        } else {
            im::service::message::MessagePacketDispatcher dispatcher(message_service);
            result = dispatcher.handle(packet);
        }

        if (push_notifier && result.push_event().enabled()) {
            im::service::push::PushContext context;
            context.sender_uid = result.push_event().sender_uid();
            context.conversation_type = result.push_event().conversation_type();
            context.conversation_id = result.push_event().conversation_id();
            push_notifier->notify_user(result.push_event().receiver_uid(),
                                       result.push_event().msg_id(),
                                       result.push_event().content(),
                                       context);
        }

        const bool ok = result.base().error_code() == im::base::SUCCESS;
        const int http_status = result.http_status() != 0
                ? result.http_status()
                : (ok ? 200 : http_status_from_error(result.base().error_code()));
        return ProcessorResult::http(http_status, result.payload());
    } catch (const std::exception& e) {
        LogManager::GetLogger("gateway_command_handlers")
                ->error("handle_send_message_http exception: {}", e.what());
        return error_response(500, "Internal server error");
    }
}

ProcessorResult handle_message_packet_http(const UnifiedMessage& msg,
                                           im::service::message::MessageService* message_service,
                                           MessageServiceAdapter* remote_message_adapter,
                                           MultiPlatformAuthManager* auth_mgr) {
    auto ready = require_http_dependency(msg, message_service || remote_message_adapter,
                                     "Message service is not initialized");
    if (ready.status_code != 0 || ready.http_status != 0) {
        return ready;
    }
    UserTokenInfo user_info;
    auto auth = verify_http_user(msg, auth_mgr, user_info);
    if (auth.status_code != 0 || auth.http_status != 0) {
        return auth;
    }

    try {
        im::message::MessagePacketRequest packet;
        packet.mutable_header()->CopyFrom(msg.get_header());
        packet.mutable_header()->set_from_uid(user_info.user_id);
        packet.mutable_header()->set_device_id(user_info.device_id);
        packet.mutable_header()->set_platform(user_info.platform);
        packet.set_type_name("application/json");
        packet.set_payload(msg.get_session_context().raw_body.empty()
                ? msg.get_json_body()
                : msg.get_session_context().raw_body);

        im::message::MessagePacketResponse result;
        if (remote_message_adapter) {
            result = remote_message_adapter->forward(packet);
        } else {
            im::service::message::MessagePacketDispatcher dispatcher(message_service);
            result = dispatcher.handle(packet);
        }

        const bool ok = result.base().error_code() == im::base::SUCCESS;
        const int http_status = result.http_status() != 0
                ? result.http_status()
                : (ok ? 200 : http_status_from_error(result.base().error_code()));
        return ProcessorResult::http(http_status, result.payload());
    } catch (const std::exception& e) {
        LogManager::GetLogger("gateway_command_handlers")
                ->error("handle_message_packet_http exception: {}", e.what());
        return error_response(500, "Internal server error");
    }
}

} // namespace

void register_message_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context) {
    const auto* runtime = context.runtime;
    auto* message_service = runtime ? runtime->message() : nullptr;
    auto* remote_message_adapter = runtime ? runtime->remote_message() : nullptr;
    auto* push_notifier = runtime ? runtime->push_notifier : nullptr;
    auto* message_ws = runtime ? runtime->message_ws : nullptr;
    auto* auth_mgr = runtime ? runtime->auth_mgr : nullptr;

    register_handler(processor, im::command::CMD_SEND_MESSAGE,
        [message_service, remote_message_adapter, push_notifier, message_ws, auth_mgr](
                const UnifiedMessage& msg) {
            if (msg.is_websocket()) {
#ifdef IM_ENABLE_MESSAGE_WS
                if (message_ws) {
                    return message_ws->handle_send(msg);
                }
#endif
                return ProcessorResult(im::base::ErrorCode::SERVER_ERROR,
                                       "WebSocket message handler is not initialized");
            }
            return handle_send_message_http(
                    msg, message_service, remote_message_adapter, auth_mgr, push_notifier);
        });

    register_handler(processor, im::command::CMD_MESSAGE_HISTORY,
        [message_service, remote_message_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_message_packet_http(msg, message_service, remote_message_adapter, auth_mgr);
        });
    register_handler(processor, im::command::CMD_PULL_MESSAGE,
        [message_service, remote_message_adapter, auth_mgr](const UnifiedMessage& msg) {
            return handle_message_packet_http(msg, message_service, remote_message_adapter, auth_mgr);
        });
}

} // namespace im::gateway
