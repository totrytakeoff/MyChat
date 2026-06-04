#ifndef GATEWAY_PUSH_SERVICE_HPP
#define GATEWAY_PUSH_SERVICE_HPP

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>

#include "../../common/proto/base.pb.h"
#include "../../common/proto/command.pb.h"
#include "connection_manager/connection_manager.hpp"
#include "../../common/network/websocket_server.hpp"

namespace im::service::message {
class MessageService;
}

namespace im::gateway {

// Abstract fanout policy.
//
// Input is every active session currently known for a user. Output is the
// session-id subset that should receive this push. Policies must not mutate
// ConnectionManager state.
class FanoutPolicy {
public:
    virtual ~FanoutPolicy() = default;
    virtual std::vector<std::string> select_sessions(
        const std::vector<DeviceSessionInfo>& sessions) = 0;
};

// Default policy: push to all active sessions.
class AllSessionsFanoutPolicy : public FanoutPolicy {
public:
    std::vector<std::string> select_sessions(
        const std::vector<DeviceSessionInfo>& sessions) override;
};

class PushService {
public:
    PushService(ConnectionManager* conn_mgr,
                im::network::WebSocketServer* ws_server,
                std::shared_ptr<im::service::message::MessageService> msg_service);

    void set_fanout_policy(std::unique_ptr<FanoutPolicy> policy);

    // Push a CMD_PUSH_MESSAGE to the recipient's selected sessions.
    //
    // Best-effort semantics:
    // - returns silently when dependencies are absent, the user is offline, or
    //   the fanout policy selects no sessions;
    // - marks the message delivered only after at least one session accepts the
    //   encoded push payload.
    void push_to_user(const std::string& receiver_uid,
                      uint64_t msg_id,
                      const std::string& content);

private:
    ConnectionManager* conn_mgr_;
    im::network::WebSocketServer* ws_server_;
    std::shared_ptr<im::service::message::MessageService> msg_service_;
    std::unique_ptr<FanoutPolicy> fanout_policy_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::gateway

#endif // GATEWAY_PUSH_SERVICE_HPP
