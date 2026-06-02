#include "message_service.hpp"
#include "message_repository.hpp"

#include <message.hpp>

namespace im {
namespace service {
namespace message {

MessageService::MessageService(std::shared_ptr<odb::pgsql::database> db)
    : db_(std::move(db))
    , repo_(std::make_unique<MessageRepository>(db_)) {}

MessageService::~MessageService() = default;

SendResult MessageService::send_text_message(const SendRequest& request) {
    SendResult result;

    if (request.sender_uid.empty()) {
        result.error_code = "EMPTY_SENDER";
        result.message = "Sender UID must not be empty";
        return result;
    }

    if (request.receiver_uid.empty()) {
        result.error_code = "EMPTY_RECEIVER";
        result.message = "Receiver UID must not be empty";
        return result;
    }

    if (request.content.empty()) {
        result.error_code = "EMPTY_CONTENT";
        result.message = "Message content must not be empty";
        return result;
    }

    Message msg(request.sender_uid, request.receiver_uid,
                request.content, request.msg_type,
                request.now_ms);

    if (!repo_->create(msg)) {
        result.error_code = "PERSIST_FAILED";
        result.message = "Failed to persist message";
        return result;
    }

    auto persisted = repo_->find_by_id(msg.msg_id());
    if (!persisted.has_value()) {
        result.error_code = "PERSIST_FAILED";
        result.message = "Message persisted but could not be read back";
        return result;
    }

    result.ok = true;
    result.data = to_data(persisted.value());
    return result;
}

std::vector<MessageData> MessageService::get_conversation(
    const std::string& user_a,
    const std::string& user_b,
    int64_t before_time,
    int limit)
{
    std::vector<MessageData> results;
    auto msgs = repo_->find_conversation(user_a, user_b,
                                          before_time > 0 ? before_time : INT64_MAX,
                                          limit > 0 ? limit : 50);
    for (const auto& msg : msgs) {
        results.push_back(to_data(msg));
    }
    return results;
}

std::vector<MessageData> MessageService::pull_offline(
    const std::string& receiver_uid,
    int64_t before_time,
    int limit)
{
    std::vector<MessageData> results;
    auto msgs = repo_->find_offline(receiver_uid,
                                     before_time > 0 ? before_time : INT64_MAX,
                                     limit > 0 ? limit : 50);
    for (const auto& msg : msgs) {
        results.push_back(to_data(msg));
    }
    return results;
}

bool MessageService::mark_delivered(uint64_t msg_id, int64_t delivered_time) {
    return repo_->mark_delivered(msg_id, delivered_time);
}

bool MessageService::mark_read(uint64_t msg_id, int64_t read_time) {
    return repo_->mark_read(msg_id, read_time);
}

MessageData MessageService::to_data(const Message& msg) const {
    MessageData data;
    data.msg_id = msg.msg_id();
    data.sender_uid = msg.sender_uid();
    data.receiver_uid = msg.receiver_uid();
    data.content = msg.content();
    data.msg_type = msg.msg_type();
    data.status = msg.status();
    data.create_time = msg.create_time();
    data.delivered_time = msg.delivered_time();
    data.read_time = msg.read_time();
    return data;
}

} // namespace message
} // namespace service
} // namespace im
