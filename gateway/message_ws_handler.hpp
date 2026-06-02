#ifndef GATEWAY_MESSAGE_WS_HANDLER_HPP
#define GATEWAY_MESSAGE_WS_HANDLER_HPP

#include <memory>
#include <string>

#include <spdlog/logger.h>

#include "../common/proto/base.pb.h"
#include "../common/proto/command.pb.h"
#include "../common/proto/message.pb.h"
#include "auth/multi_platform_auth.hpp"
#include "message_processor/unified_message.hpp"
#include "message_processor/message_processor.hpp"

namespace im::service::message {
class MessageService;
}

namespace im::gateway {

class ConnectionManager;

} // namespace im::gateway

namespace im::network {
class WebSocketServer;
} // namespace im::network

namespace im::gateway {

class MessageWsHandler {
public:
    MessageWsHandler(
        std::shared_ptr<im::service::message::MessageService> msg_service,
        std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
        ConnectionManager* conn_mgr = nullptr,
        im::network::WebSocketServer* ws_server = nullptr);

    ProcessorResult handle_send(const UnifiedMessage& msg);

private:
    void try_push_to_recipient(const std::string& receiver_uid,
                               uint64_t msg_id,
                               const std::string& content);

    std::shared_ptr<im::service::message::MessageService> msg_service_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    ConnectionManager* conn_mgr_;
    im::network::WebSocketServer* ws_server_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::gateway

#endif // GATEWAY_MESSAGE_WS_HANDLER_HPP
