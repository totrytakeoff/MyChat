#ifndef GATEWAY_MESSAGE_WS_HANDLER_HPP
#define GATEWAY_MESSAGE_WS_HANDLER_HPP

#include <memory>
#include <string>

#include <spdlog/logger.h>

#include "../../common/proto/base.pb.h"
#include "../../common/proto/command.pb.h"
#include "../../services/message/message_packet_dispatcher.hpp"
#include "auth/multi_platform_auth.hpp"
#include "message_processor/unified_message.hpp"
#include "message_processor/message_processor.hpp"

namespace im::service::push {
class PushNotifier;
}

namespace im::service::message {
class MessageService;
}

namespace im::gateway {
class MessageServiceAdapter;

// Handles CMD_SEND_MESSAGE over WebSocket.
//
// Input is a UnifiedMessage produced by the Gateway parser. The handler
// verifies token-derived sender identity, forwards packet data to the message
// service boundary, returns a protobuf ack, and delegates best-effort recipient
// delivery to PushService when available.
class MessageWsHandler {
public:
    MessageWsHandler(
        std::shared_ptr<im::service::message::MessageService> message_service,
        std::shared_ptr<MessageServiceAdapter> remote_message_adapter,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
        im::service::push::PushNotifier* push_notifier = nullptr);

    ProcessorResult handle_send(const UnifiedMessage& msg);

private:
    std::shared_ptr<im::service::message::MessageService> message_service_;
    std::unique_ptr<im::service::message::MessagePacketDispatcher> local_dispatcher_;
    std::shared_ptr<MessageServiceAdapter> remote_message_adapter_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    im::service::push::PushNotifier* push_notifier_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::gateway

#endif // GATEWAY_MESSAGE_WS_HANDLER_HPP
