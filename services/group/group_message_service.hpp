#ifndef IM_SERVICE_GROUP_GROUP_MESSAGE_SERVICE_HPP
#define IM_SERVICE_GROUP_GROUP_MESSAGE_SERVICE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace odb {
namespace pgsql {
class database;
}
}

namespace im {
namespace service {
namespace user {
class UserService;
}

namespace group {

class GroupService;

class GroupRepository;

struct GroupSendResult {
    bool ok = false;
    std::string error_code;
    std::string message;
    uint64_t msg_id = 0;
};

struct GroupMessageDTO {
    uint64_t msg_id;
    std::string sender_uid;
    std::string sender_nickname;
    std::string content;
    int64_t created_at;
};

class GroupMessageService {
public:
    GroupMessageService(std::shared_ptr<odb::pgsql::database> db,
                        std::shared_ptr<im::service::user::UserService> user_service,
                        std::shared_ptr<GroupService> group_service);
    ~GroupMessageService();

    GroupSendResult send_message(uint64_t group_id, const std::string& sender_uid,
                                 const std::string& content, int64_t now_ms);
    std::vector<GroupMessageDTO> get_history(uint64_t group_id, int64_t before_time,
                                              int limit = 50);

private:
    std::string resolve_nickname(const std::string& uid);

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::shared_ptr<GroupService> group_service_;
    std::unique_ptr<class GroupMessageRepository> repo_;
};

} // namespace group
} // namespace service
} // namespace im

#endif // IM_SERVICE_GROUP_GROUP_MESSAGE_SERVICE_HPP
