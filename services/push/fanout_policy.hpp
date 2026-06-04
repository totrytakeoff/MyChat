#ifndef SERVICES_PUSH_FANOUT_POLICY_HPP
#define SERVICES_PUSH_FANOUT_POLICY_HPP

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace im::service::push {

struct PushSessionInfo {
    std::string session_id;
    std::string platform;
    std::chrono::system_clock::time_point connect_time;
};

// Selects the session-id subset that should receive a push.
class FanoutPolicy {
public:
    virtual ~FanoutPolicy() = default;

    virtual std::vector<std::string> select_sessions(
        const std::vector<PushSessionInfo>& sessions) = 0;
};

// Default policy: push to all active sessions.
class AllSessionsFanoutPolicy : public FanoutPolicy {
public:
    std::vector<std::string> select_sessions(
        const std::vector<PushSessionInfo>& sessions) override;
};

// Selects sessions whose platform matches one of the allowed platforms.
class PlatformFilterFanoutPolicy : public FanoutPolicy {
public:
    explicit PlatformFilterFanoutPolicy(std::vector<std::string> allowed_platforms);

    std::vector<std::string> select_sessions(
        const std::vector<PushSessionInfo>& sessions) override;

private:
    std::vector<std::string> allowed_platforms_;
};

// Selects the single most recently connected session.
class NewestSessionFanoutPolicy : public FanoutPolicy {
public:
    std::vector<std::string> select_sessions(
        const std::vector<PushSessionInfo>& sessions) override;
};

} // namespace im::service::push

#endif // SERVICES_PUSH_FANOUT_POLICY_HPP
