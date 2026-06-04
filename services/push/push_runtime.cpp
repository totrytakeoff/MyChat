#include "push_runtime.hpp"

#include <chrono>
#include <utility>

#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/command.pb.h"
#include "../../common/proto/push.pb.h"
#include "../../common/utils/log_manager.hpp"
#include "../../common/utils/service_identity.hpp"

namespace im::service::push {

using im::network::ProtobufCodec;
using im::utils::LogManager;
using im::utils::ServiceIdentityManager;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // anonymous namespace

PushRuntime::PushRuntime(PushSessionProvider* session_provider,
                         PushPayloadSender* payload_sender,
                         PushDeliveryMarker* delivery_marker)
    : session_provider_(session_provider)
    , payload_sender_(payload_sender)
    , delivery_marker_(delivery_marker)
    , fanout_policy_(std::make_unique<AllSessionsFanoutPolicy>())
{
    LogManager::SetLogToFile("push_runtime", "logs/push_runtime.log");
    logger_ = LogManager::GetLogger("push_runtime");
}

void PushRuntime::set_fanout_policy(std::unique_ptr<FanoutPolicy> policy) {
    fanout_policy_ = std::move(policy);
}

void PushRuntime::notify_user(const std::string& receiver_uid,
                              uint64_t msg_id,
                              const std::string& content) {
    if (!session_provider_ || !payload_sender_ || !delivery_marker_ || !fanout_policy_) {
        logger_->debug("Push skipped: one or more runtime deps are null");
        return;
    }

    try {
        auto sessions = session_provider_->get_sessions(receiver_uid);
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

        auto payload = build_payload(receiver_uid, msg_id, content);
        if (payload.empty()) {
            logger_->warn("Failed to encode push message for receiver {}", receiver_uid);
            return;
        }

        int success_count = 0;
        for (const auto& session_id : selected) {
            try {
                if (payload_sender_->send_payload(session_id, payload)) {
                    ++success_count;
                }
            } catch (const std::exception& e) {
                logger_->warn("Push to session {} failed: {}", session_id, e.what());
            }
        }

        if (success_count > 0) {
            delivery_marker_->mark_delivered(msg_id, now_ms());
            logger_->info("Pushed message {} to {}/{} sessions of user {}, marked delivered",
                          msg_id, success_count, selected.size(), receiver_uid);
        } else {
            logger_->info("No session accepted push for message {} to user {}, stays undelivered",
                          msg_id, receiver_uid);
        }
    } catch (const std::exception& e) {
        logger_->error("Exception in PushRuntime::notify_user: {}", e.what());
    }
}

std::string PushRuntime::build_payload(const std::string& receiver_uid,
                                       uint64_t msg_id,
                                       const std::string& content) const {
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
        return "";
    }
    return encoded;
}

} // namespace im::service::push
