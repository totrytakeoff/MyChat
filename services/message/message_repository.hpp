#ifndef IM_SERVICE_MESSAGE_MESSAGE_REPOSITORY_HPP
#define IM_SERVICE_MESSAGE_MESSAGE_REPOSITORY_HPP

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

class Message;

class MessageRepository {
public:
    explicit MessageRepository(std::shared_ptr<odb::pgsql::database> db);

    bool create(Message& msg);
    std::optional<Message> find_by_id(uint64_t msg_id);
    std::vector<Message> find_conversation(const std::string& user_a,
                                            const std::string& user_b,
                                            int64_t before_time,
                                            int limit);
    std::vector<Message> find_offline(const std::string& receiver_uid,
                                       int64_t before_time,
                                       int limit);
    bool mark_delivered(uint64_t msg_id, int64_t delivered_time);
    bool mark_read(uint64_t msg_id, int64_t read_time);

private:
    std::shared_ptr<odb::pgsql::database> db_;
};

} // namespace message
} // namespace service
} // namespace im

#endif // IM_SERVICE_MESSAGE_MESSAGE_REPOSITORY_HPP
