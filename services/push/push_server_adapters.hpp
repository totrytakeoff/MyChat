#ifndef SERVICES_PUSH_PUSH_SERVER_ADAPTERS_HPP
#define SERVICES_PUSH_PUSH_SERVER_ADAPTERS_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>

#include "push_runtime.hpp"

namespace im::service::push {

// Transitional standalone-server adapters.
//
// The current Gateway process still owns live WebSocket sessions. Until a
// cross-process session/payload channel is added, the standalone Push server
// should accept NotifyUser RPCs without pretending it can deliver to Gateway
// sessions.
class EmptyPushSessionProvider final : public PushSessionProvider {
public:
    std::vector<PushSessionInfo> get_sessions(const std::string& receiver_uid) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

class NoopPushPayloadSender final : public PushPayloadSender {
public:
    bool send_payload(const std::string& session_id,
                      const std::string& payload) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

class NoopPushDeliveryMarker final : public PushDeliveryMarker {
public:
    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::service::push

#endif // SERVICES_PUSH_PUSH_SERVER_ADAPTERS_HPP
