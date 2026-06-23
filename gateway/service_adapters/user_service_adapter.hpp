#ifndef GATEWAY_SERVICE_ADAPTERS_USER_SERVICE_ADAPTER_HPP
#define GATEWAY_SERVICE_ADAPTERS_USER_SERVICE_ADAPTER_HPP

#include "user.pb.h"

namespace im::gateway {

class UserServiceAdapter {
public:
    virtual ~UserServiceAdapter() = default;

    virtual im::user::UserPacketResponse forward(
        const im::user::UserPacketRequest& packet) = 0;
};

}  // namespace im::gateway

#endif  // GATEWAY_SERVICE_ADAPTERS_USER_SERVICE_ADAPTER_HPP
