#include "push_service.hpp"

#include <chrono>
#include <string>

#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/push.pb.h"
#include "../../common/utils/log_manager.hpp"
#include "../../common/utils/service_identity.hpp"
#include "../../services/message/message_service.hpp"

namespace im::gateway {

using im::network::ProtobufCodec;
using im::service::message::MessageService;
using im::utils::LogManager;
using im::utils::ServiceIdentityManager;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // anonymous namespace

// --- AllSessionsFanoutPolicy ---

std::vector<std::string> AllSessionsFanoutPolicy::select_sessions(
    const std::vector<DeviceSessionInfo>& sessions)
{
    std::vector<std::string> ids;
    ids.reserve(sessions.size());
    for (const auto& s : sessions) {
        ids.push_back(s.session_id);
    }
    return ids;
}

// --- PushService ---

PushService::PushService(ConnectionManager* conn_mgr,
                         im::network::WebSocketServer* ws_server,
                         std::shared_ptr<MessageService> msg_service)
    : conn_mgr_(conn_mgr)
    , ws_server_(ws_server)
    , msg_service_(std::move(msg_service))
    , fanout_policy_(std::make_unique<AllSessionsFanoutPolicy>())
{
    LogManager::SetLogToFile("push_service", "logs/push_service.log");
    logger_ = LogManager::GetLogger("push_service");
}

void PushService::set_fanout_policy(std::unique_ptr<FanoutPolicy> policy) {
    fanout_policy_ = std::move(policy);
}

void PushService::push_to_user(const std::string& receiver_uid,
                               uint64_t msg_id,
                               const std::string& content) {
    if (!conn_mgr_ || !ws_server_ || !msg_service_ || !fanout_policy_) {
        logger_->debug("Push skipped: one or more deps are null");
        return;
    }

    try {
        auto sessions = conn_mgr_->get_user_sessions(receiver_uid);
        if (sessions.empty()) {
            logger_->debug("No online sessions for receiver {}, message stays undelivered",
                           receiver_uid);
            return;
        }

        auto selected = fanout_policy_->select_sessions(sessions);
        if (selected.empty()) {
            logger_->debug("Fanout policy selected 0 sessions for user {}", receiver_uid);
            return;
        }

        // Build the push envelope: IMHeader + PushRequest
        im::base::IMHeader push_header;
        push_header.set_version("1.0");
        push_header.set_cmd_id(im::command::CMD_PUSH_MESSAGE);
        push_header.set_from_uid(ServiceIdentityManager::getInstance().getDeviceId());
        push_header.set_to_uid(receiver_uid);
        push_header.set_timestamp(static_cast<uint64_t>(now_ms()));

        im::push::PushRequest push_req;
        auto* push_body = push_req.mutable_body();
        push_body->set_type(im::push::PUSH_MESSAGE);
        push_body->set_content(content);
        push_body->set_related_message_id(std::to_string(msg_id));

        std::string encoded;
        if (!ProtobufCodec::encode(push_header, push_req, encoded)) {
            logger_->warn("Failed to encode push message for receiver {}", receiver_uid);
            return;
        }

        int success_count = 0;
        for (const auto& session_id : selected) {
            try {
                auto session = ws_server_->get_session(session_id);
                if (session) {
                    session->send(encoded);
                    ++success_count;
                }
            } catch (const std::exception& e) {
                logger_->warn("Push to session {} failed: {}", session_id, e.what());
            }
        }

        if (success_count > 0) {
            msg_service_->mark_delivered(msg_id, now_ms());
            logger_->info("Pushed message {} to {}/{} sessions of user {}, marked delivered",
                          msg_id, success_count, selected.size(), receiver_uid);
        } else {
            logger_->info("No session accepted push for message {} to user {}, stays undelivered",
                          msg_id, receiver_uid);
        }
    } catch (const std::exception& e) {
        logger_->error("Exception in push_to_user: {}", e.what());
    }
}

} // namespace im::gateway
