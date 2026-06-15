#ifndef GATEWAY_PUSH_SERVICE_HPP
#define GATEWAY_PUSH_SERVICE_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "connection_manager/connection_manager.hpp"
#include "../../common/network/websocket_server.hpp"
#include "../../services/push/fanout_policy.hpp"
#include "../../services/push/push_notifier.hpp"
#include "../../services/push/push_runtime.hpp"

namespace im::gateway {
class MessageClient;

class PushService : public im::service::push::PushNotifier,
                    public im::service::push::PushSessionProvider,
                    public im::service::push::PushPayloadSender,
                    public im::service::push::PushDeliveryMarker {
public:
    PushService(ConnectionManager* conn_mgr,
                im::network::WebSocketServer* ws_server,
                std::shared_ptr<MessageClient> msg_client);

    void set_fanout_policy(std::unique_ptr<im::service::push::FanoutPolicy> policy);

    // Push a CMD_PUSH_MESSAGE to the recipient's selected sessions.
    //
    // Best-effort semantics:
    // - returns silently when dependencies are absent, the user is offline, or
    //   the fanout policy selects no sessions;
    // - marks the message delivered only after at least one session accepts the
    //   encoded push payload.
    void push_to_user(const std::string& receiver_uid,
                      uint64_t msg_id,
                      const std::string& content,
                      const im::service::push::PushContext& context =
                          im::service::push::PushContext{});

    void notify_user(const std::string& receiver_uid,
                     uint64_t msg_id,
                     const std::string& content,
                     const im::service::push::PushContext& context =
                         im::service::push::PushContext{}) override;

    std::vector<im::service::push::PushSessionInfo> get_sessions(
        const std::string& receiver_uid) override;

    bool send_payload(const std::string& session_id,
                      const std::string& payload) override;

    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override;

private:
    ConnectionManager* conn_mgr_;
    im::network::WebSocketServer* ws_server_;
    std::shared_ptr<MessageClient> msg_client_;
    im::service::push::PushRuntime runtime_;
};

} // namespace im::gateway

#endif // GATEWAY_PUSH_SERVICE_HPP
