#ifndef GATEWAY_SERVICE_ADAPTERS_FRIEND_SERVICE_ADAPTER_HPP
#define GATEWAY_SERVICE_ADAPTERS_FRIEND_SERVICE_ADAPTER_HPP

#include "friend.pb.h"

namespace im::gateway {

class FriendServiceAdapter {
public:
    virtual ~FriendServiceAdapter() = default;

    virtual im::friend_::FriendPacketResponse forward(
        const im::friend_::FriendPacketRequest& packet) = 0;
};

}  // namespace im::gateway

#endif  // GATEWAY_SERVICE_ADAPTERS_FRIEND_SERVICE_ADAPTER_HPP
