#ifndef GATEWAY_FANOUT_POLICIES_HPP
#define GATEWAY_FANOUT_POLICIES_HPP

#include <string>
#include <vector>

#include "push_service.hpp"

namespace im::gateway {

// Selects sessions whose platform matches one of the allowed platforms.
// Typical use: push only to mobile devices, or only to web clients.
class PlatformFilterFanoutPolicy : public FanoutPolicy {
public:
    explicit PlatformFilterFanoutPolicy(std::vector<std::string> allowed_platforms);

    std::vector<std::string> select_sessions(
        const std::vector<DeviceSessionInfo>& sessions) override;

private:
    std::vector<std::string> allowed_platforms_;
};

// Selects the single most recently connected session.
// Useful for notification-type pushes that only need to reach one device.
class NewestSessionFanoutPolicy : public FanoutPolicy {
public:
    std::vector<std::string> select_sessions(
        const std::vector<DeviceSessionInfo>& sessions) override;
};

} // namespace im::gateway

#endif // GATEWAY_FANOUT_POLICIES_HPP
