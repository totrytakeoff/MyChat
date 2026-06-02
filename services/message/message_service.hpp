#ifndef IM_SERVICE_MESSAGE_MESSAGE_SERVICE_HPP
#define IM_SERVICE_MESSAGE_MESSAGE_SERVICE_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cstdint>

namespace odb {
namespace pgsql {
class database;
}
}

namespace im {
namespace service {
namespace message {

enum class MessageStatus;
enum class MessageType;
class Message;

struct MessageData {
    uint64_t msg_id = 0;
    std::string sender_uid;
    std::string receiver_uid;
    std::string content;
    MessageType msg_type;
    MessageStatus status;
    int64_t create_time = 0;
    int64_t delivered_time = 0;
    int64_t read_time = 0;
};

struct SendRequest {
    std::string sender_uid;
    std::string receiver_uid;
    std::string content;
    MessageType msg_type;
    int64_t now_ms = 0;
};

struct SendResult {
    bool ok = false;
    std::string error_code;
    std::string message;
    MessageData data;
};

class MessageRepository;

class MessageService {
public:
    explicit MessageService(std::shared_ptr<odb::pgsql::database> db);
    ~MessageService();

    SendResult send_text_message(const SendRequest& request);
    std::vector<MessageData> get_conversation(const std::string& user_a,
                                               const std::string& user_b,
                                               int64_t before_time,
                                               int limit);
    std::vector<MessageData> pull_offline(const std::string& receiver_uid,
                                           int64_t before_time,
                                           int limit);
    bool mark_delivered(uint64_t msg_id, int64_t delivered_time);
    bool mark_read(uint64_t msg_id, int64_t read_time);

private:
    MessageData to_data(const Message& msg) const;

    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<MessageRepository> repo_;
};

} // namespace message
} // namespace service
} // namespace im

#endif // IM_SERVICE_MESSAGE_MESSAGE_SERVICE_HPP
