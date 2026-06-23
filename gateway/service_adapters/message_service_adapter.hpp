#ifndef GATEWAY_SERVICE_ADAPTERS_MESSAGE_SERVICE_ADAPTER_HPP
#define GATEWAY_SERVICE_ADAPTERS_MESSAGE_SERVICE_ADAPTER_HPP

#include "message.pb.h"

namespace im::gateway {

class MessageServiceAdapter {
public:
    virtual ~MessageServiceAdapter() = default;

    virtual im::message::MessagePacketResponse forward(
        const im::message::MessagePacketRequest& packet) = 0;
};

}  // namespace im::gateway

#endif  // GATEWAY_SERVICE_ADAPTERS_MESSAGE_SERVICE_ADAPTER_HPP
