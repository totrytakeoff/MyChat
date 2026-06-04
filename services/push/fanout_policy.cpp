#include "fanout_policy.hpp"

#include <utility>

namespace im::service::push {

std::vector<std::string> AllSessionsFanoutPolicy::select_sessions(
    const std::vector<PushSessionInfo>& sessions)
{
    std::vector<std::string> ids;
    ids.reserve(sessions.size());
    for (const auto& session : sessions) {
        ids.push_back(session.session_id);
    }
    return ids;
}

PlatformFilterFanoutPolicy::PlatformFilterFanoutPolicy(
    std::vector<std::string> allowed_platforms)
    : allowed_platforms_(std::move(allowed_platforms))
{}

std::vector<std::string> PlatformFilterFanoutPolicy::select_sessions(
    const std::vector<PushSessionInfo>& sessions)
{
    std::vector<std::string> ids;
    for (const auto& session : sessions) {
        for (const auto& platform : allowed_platforms_) {
            if (session.platform == platform) {
                ids.push_back(session.session_id);
                break;
            }
        }
    }
    return ids;
}

std::vector<std::string> NewestSessionFanoutPolicy::select_sessions(
    const std::vector<PushSessionInfo>& sessions)
{
    if (sessions.empty()) {
        return {};
    }

    const auto* newest = &sessions[0];
    for (std::size_t i = 1; i < sessions.size(); ++i) {
        if (sessions[i].connect_time > newest->connect_time) {
            newest = &sessions[i];
        }
    }
    return {newest->session_id};
}

} // namespace im::service::push
