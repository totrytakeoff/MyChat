#include "gateway_command_handler_modules.hpp"

#include <exception>
#include <string>

#include "../../common/proto/command.pb.h"
#include "../../common/utils/log_manager.hpp"
#include "../../services/group/group_message_service.hpp"
#include "../../services/group/group_packet_dispatcher.hpp"
#include "../../services/group/group_service.hpp"
#include "../../services/push/push_notifier.hpp"
#include "../service_adapters/group_service_adapter.hpp"
#include "command_handler_utils.hpp"

namespace im::gateway {
namespace {

using namespace im::gateway::command_handlers;
using im::utils::LogManager;

im::group::GroupPacketRequest make_group_packet(const UnifiedMessage& msg) {
    im::group::GroupPacketRequest packet;
    packet.mutable_header()->CopyFrom(msg.get_header());
    packet.set_type_name("application/json");
    packet.set_payload(msg.get_session_context().raw_body.empty()
            ? msg.get_json_body()
            : msg.get_session_context().raw_body);
    return packet;
}

void notify_group_push_events(const im::group::GroupPacketResponse& result,
                              im::service::push::PushNotifier* push_notifier) {
    if (!push_notifier) {
        return;
    }
    for (const auto& event : result.push_events()) {
        if (!event.enabled()) {
            continue;
        }
        im::service::push::PushContext context;
        context.sender_uid = event.sender_uid();
        context.conversation_type = event.conversation_type();
        context.conversation_id = event.conversation_id();
        push_notifier->notify_user(event.receiver_uid(),
                                   event.msg_id(),
                                   event.content(),
                                   context);
    }
}

ProcessorResult handle_group_packet_http(
        const UnifiedMessage& msg,
        im::service::group::GroupService* group_service,
        im::service::group::GroupMessageService* group_message_service,
        GroupServiceAdapter* remote_group_adapter,
        MultiPlatformAuthManager* auth_mgr,
        im::service::push::PushNotifier* push_notifier) {
    auto ready = require_http_dependency(
            msg,
            remote_group_adapter || group_service,
            "Group service is not initialized");
    if (ready.status_code != 0 || ready.http_status != 0) {
        return ready;
    }

    UserTokenInfo user_info;
    auto auth = verify_http_user(msg, auth_mgr, user_info);
    if (auth.status_code != 0 || auth.http_status != 0) {
        return auth;
    }

    try {
        im::group::GroupPacketRequest packet = make_group_packet(msg);
        packet.mutable_header()->set_from_uid(user_info.user_id);
        packet.mutable_header()->set_device_id(user_info.device_id);
        packet.mutable_header()->set_platform(user_info.platform);

        im::group::GroupPacketResponse result;
        if (remote_group_adapter) {
            result = remote_group_adapter->forward(packet);
        } else {
            im::service::group::GroupPacketDispatcher dispatcher(
                    group_service, group_message_service);
            result = dispatcher.handle(packet);
        }

        notify_group_push_events(result, push_notifier);

        const bool ok = result.base().error_code() == im::base::SUCCESS;
        const int http_status = result.http_status() != 0
                ? result.http_status()
                : (ok ? 200 : http_status_from_error(result.base().error_code()));
        return ProcessorResult::http(http_status, result.payload());
    } catch (const std::exception& e) {
        LogManager::GetLogger("gateway_command_handlers")
                ->error("handle_group_packet_http exception: {}", e.what());
        return error_response(500, "Internal server error");
    }
}

} // namespace

void register_group_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context) {
    const auto* runtime = context.runtime;
    auto* group_service = runtime ? runtime->group() : nullptr;
    auto* group_message_service = runtime ? runtime->group_message() : nullptr;
    auto* remote_group_adapter = runtime ? runtime->remote_group() : nullptr;
    auto* push_notifier = runtime ? runtime->push_notifier : nullptr;
    auto* auth_mgr = runtime ? runtime->auth_mgr : nullptr;

    register_handler(processor, im::command::CMD_CREATE_GROUP,
        [group_service, group_message_service, remote_group_adapter, auth_mgr](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, nullptr);
        });
    register_handler(processor, im::command::CMD_SEARCH_GROUP,
        [group_service, group_message_service, remote_group_adapter, auth_mgr](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, nullptr);
        });
    register_handler(processor, im::command::CMD_GET_GROUP_INFO,
        [group_service, group_message_service, remote_group_adapter, auth_mgr](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, nullptr);
        });
    register_handler(processor, im::command::CMD_GET_GROUP_LIST,
        [group_service, group_message_service, remote_group_adapter, auth_mgr](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, nullptr);
        });
    register_handler(processor, im::command::CMD_APPLY_JOIN_GROUP,
        [group_service, group_message_service, remote_group_adapter, auth_mgr](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, nullptr);
        });
    register_handler(processor, im::command::CMD_QUIT_GROUP,
        [group_service, group_message_service, remote_group_adapter, auth_mgr](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, nullptr);
        });
    register_handler(processor, im::command::CMD_GET_GROUP_MEMBERS,
        [group_service, group_message_service, remote_group_adapter, auth_mgr](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, nullptr);
        });

    register_handler(processor, im::command::CMD_SEND_GROUP_MESSAGE,
        [group_service, group_message_service, remote_group_adapter, auth_mgr, push_notifier](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, push_notifier);
        });
    register_handler(processor, im::command::CMD_GET_GROUP_MESSAGES,
        [group_service, group_message_service, remote_group_adapter, auth_mgr](
                const UnifiedMessage& msg) {
            return handle_group_packet_http(
                    msg, group_service, group_message_service,
                    remote_group_adapter, auth_mgr, nullptr);
        });
}

} // namespace im::gateway
