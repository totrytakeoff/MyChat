#ifndef SERVICES_PUSH_PUSH_RUNTIME_HPP
#define SERVICES_PUSH_PUSH_RUNTIME_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>

#include "fanout_policy.hpp"
#include "push_notifier.hpp"

namespace im::service::push {

class PushSessionProvider {
public:
    virtual ~PushSessionProvider() = default;

    virtual std::vector<PushSessionInfo> get_sessions(
        const std::string& receiver_uid) = 0;
};

class PushPayloadSender {
public:
    virtual ~PushPayloadSender() = default;

    virtual bool send_payload(const std::string& session_id,
                              const std::string& payload) = 0;
};

class PushDeliveryMarker {
public:
    virtual ~PushDeliveryMarker() = default;

    virtual bool mark_delivered(uint64_t msg_id, int64_t delivered_time) = 0;
};

// Core push delivery workflow independent of Gateway runtime types.
class PushRuntime : public PushNotifier {
public:
    PushRuntime(PushSessionProvider* session_provider,
                PushPayloadSender* payload_sender,
                PushDeliveryMarker* delivery_marker);

    void set_fanout_policy(std::unique_ptr<FanoutPolicy> policy);

    void notify_user(const std::string& receiver_uid,
                     uint64_t msg_id,
                     const std::string& content) override;

private:
    std::string build_payload(const std::string& receiver_uid,
                              uint64_t msg_id,
                              const std::string& content) const;

    PushSessionProvider* session_provider_;
    PushPayloadSender* payload_sender_;
    PushDeliveryMarker* delivery_marker_;
    std::unique_ptr<FanoutPolicy> fanout_policy_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::service::push

#endif // SERVICES_PUSH_PUSH_RUNTIME_HPP
