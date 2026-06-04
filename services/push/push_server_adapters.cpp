#include "push_server_adapters.hpp"

#include <utility>

#include "../../common/utils/log_manager.hpp"

namespace im::service::push {

namespace {

std::shared_ptr<spdlog::logger> adapter_logger() {
    im::utils::LogManager::SetLogToFile("push_server_adapters",
                                        "logs/push_server_adapters.log");
    return im::utils::LogManager::GetLogger("push_server_adapters");
}

} // namespace

std::vector<PushSessionInfo> EmptyPushSessionProvider::get_sessions(
    const std::string& receiver_uid) {
    if (!logger_) {
        logger_ = adapter_logger();
    }
    logger_->debug("Standalone Push server has no session channel for receiver {}",
                   receiver_uid);
    return {};
}

bool NoopPushPayloadSender::send_payload(const std::string& session_id,
                                         const std::string& payload) {
    if (!logger_) {
        logger_ = adapter_logger();
    }
    logger_->debug("Standalone Push server dropped payload for session {} ({} bytes)",
                   session_id, payload.size());
    return false;
}

bool NoopPushDeliveryMarker::mark_delivered(uint64_t msg_id, int64_t delivered_time) {
    if (!logger_) {
        logger_ = adapter_logger();
    }
    logger_->debug("Standalone Push server did not mark message {} delivered at {}",
                   msg_id, delivered_time);
    return false;
}

} // namespace im::service::push
