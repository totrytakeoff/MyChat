#include "message_client.hpp"

#include <stdexcept>
#include <utility>

namespace im::gateway {

LocalMessageClient::LocalMessageClient(
    std::shared_ptr<im::service::message::MessageService> message_service)
    : message_service_(std::move(message_service)) {
    if (!message_service_) {
        throw std::invalid_argument("LocalMessageClient requires MessageService");
    }
}

im::service::message::SendResult LocalMessageClient::send_text_message(
    const im::service::message::SendRequest& request) {
    return message_service_->send_text_message(request);
}

std::vector<im::service::message::MessageData> LocalMessageClient::get_conversation(
    const std::string& user_a,
    const std::string& user_b,
    int64_t before_time,
    int limit) {
    return message_service_->get_conversation(user_a, user_b, before_time, limit);
}

std::vector<im::service::message::MessageData> LocalMessageClient::pull_offline(
    const std::string& receiver_uid,
    int64_t before_time,
    int limit) {
    return message_service_->pull_offline(receiver_uid, before_time, limit);
}

bool LocalMessageClient::mark_delivered(uint64_t msg_id, int64_t delivered_time) {
    return message_service_->mark_delivered(msg_id, delivered_time);
}

bool LocalMessageClient::mark_read(uint64_t msg_id, int64_t read_time) {
    return message_service_->mark_read(msg_id, read_time);
}

}  // namespace im::gateway
