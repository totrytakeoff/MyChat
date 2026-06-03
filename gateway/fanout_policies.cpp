#include "fanout_policies.hpp"

#include <algorithm>

namespace im::gateway {

// --- PlatformFilterFanoutPolicy ---

PlatformFilterFanoutPolicy::PlatformFilterFanoutPolicy(
    std::vector<std::string> allowed_platforms)
    : allowed_platforms_(std::move(allowed_platforms))
{}

std::vector<std::string> PlatformFilterFanoutPolicy::select_sessions(
    const std::vector<DeviceSessionInfo>& sessions)
{
    std::vector<std::string> ids;
    for (const auto& s : sessions) {
        for (const auto& platform : allowed_platforms_) {
            if (s.platform == platform) {
                ids.push_back(s.session_id);
                break;
            }
        }
    }
    return ids;
}

// --- NewestSessionFanoutPolicy ---

std::vector<std::string> NewestSessionFanoutPolicy::select_sessions(
    const std::vector<DeviceSessionInfo>& sessions)
{
    if (sessions.empty()) {
        return {};
    }

    const DeviceSessionInfo* newest = &sessions[0];
    for (size_t i = 1; i < sessions.size(); ++i) {
        if (sessions[i].connect_time > newest->connect_time) {
            newest = &sessions[i];
        }
    }
    return {newest->session_id};
}

} // namespace im::gateway
