#ifndef GATEWAY_COMMAND_HANDLER_MODULES_HPP
#define GATEWAY_COMMAND_HANDLER_MODULES_HPP

#include "gateway_command_handler_registry.hpp"

namespace im::gateway {

void register_user_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context);

void register_message_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context);

void register_friend_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context);

void register_group_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context);

} // namespace im::gateway

#endif // GATEWAY_COMMAND_HANDLER_MODULES_HPP
