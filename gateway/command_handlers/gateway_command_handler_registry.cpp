#include "gateway_command_handler_registry.hpp"

#include "gateway_command_handler_modules.hpp"

namespace im::gateway {

void register_gateway_command_handlers(
        MessageProcessor& processor,
        const GatewayCommandHandlerContext& context) {
    register_user_command_handlers(processor, context);
    register_message_command_handlers(processor, context);
    register_friend_command_handlers(processor, context);
    register_group_command_handlers(processor, context);
}

} // namespace im::gateway
