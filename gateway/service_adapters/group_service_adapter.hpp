#ifndef GATEWAY_SERVICE_ADAPTERS_GROUP_SERVICE_ADAPTER_HPP
#define GATEWAY_SERVICE_ADAPTERS_GROUP_SERVICE_ADAPTER_HPP

#include "group.pb.h"

namespace im::gateway {

class GroupServiceAdapter {
public:
    virtual ~GroupServiceAdapter() = default;

    virtual im::group::GroupPacketResponse forward(
        const im::group::GroupPacketRequest& packet) = 0;
};

}  // namespace im::gateway

#endif  // GATEWAY_SERVICE_ADAPTERS_GROUP_SERVICE_ADAPTER_HPP
