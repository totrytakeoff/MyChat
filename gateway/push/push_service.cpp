#include "push_service.hpp"

#include <string>

#include "../http/message_client.hpp"

namespace im::gateway {

using im::service::push::FanoutPolicy;
using im::service::push::PushSessionInfo;

// --- PushService ---

PushService::PushService(ConnectionManager* conn_mgr,
                         im::network::WebSocketServer* ws_server,
                         std::shared_ptr<MessageClient> msg_client)
    : conn_mgr_(conn_mgr)
    , ws_server_(ws_server)
    , msg_client_(std::move(msg_client))
    , runtime_(this, this, this)
{}

void PushService::set_fanout_policy(std::unique_ptr<FanoutPolicy> policy) {
    runtime_.set_fanout_policy(std::move(policy));
}

void PushService::push_to_user(const std::string& receiver_uid,
                               uint64_t msg_id,
                               const std::string& content) {
    runtime_.notify_user(receiver_uid, msg_id, content);
}

void PushService::notify_user(const std::string& receiver_uid,
                              uint64_t msg_id,
                              const std::string& content) {
    push_to_user(receiver_uid, msg_id, content);
}

std::vector<PushSessionInfo> PushService::get_sessions(const std::string& receiver_uid) {
    if (!conn_mgr_) {
        return {};
    }

    auto sessions = conn_mgr_->get_user_sessions(receiver_uid);
    std::vector<PushSessionInfo> push_sessions;
    push_sessions.reserve(sessions.size());
    for (const auto& session : sessions) {
        push_sessions.push_back({
            .session_id = session.session_id,
            .platform = session.platform,
            .connect_time = session.connect_time,
        });
    }
    return push_sessions;
}

bool PushService::send_payload(const std::string& session_id,
                               const std::string& payload) {
    if (!ws_server_) {
        return false;
    }

    auto session = ws_server_->get_session(session_id);
    if (!session) {
        return false;
    }

    session->send(payload);
    return true;
}

bool PushService::mark_delivered(uint64_t msg_id, int64_t delivered_time) {
    if (!msg_client_) {
        return false;
    }
    return msg_client_->mark_delivered(msg_id, delivered_time);
}

} // namespace im::gateway
