#ifndef IM_SERVICE_GROUP_GROUP_SERVICE_HPP
#define IM_SERVICE_GROUP_GROUP_SERVICE_HPP

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

enum class GroupRole : int;

struct CreateGroupRequest {
    std::string name;
    std::string creator_uid;
    int64_t now_ms = 0;
};

struct GroupResult {
    bool ok = false;
    std::string error_code;
    std::string message;
    uint64_t group_id = 0;
};

struct GroupInfoDTO {
    uint64_t group_id;
    std::string name;
    std::string creator_uid;
    int64_t created_at;
    int member_count = 0;
};

struct MemberInfoDTO {
    uint64_t id;
    std::string user_uid;
    std::string nickname;
    GroupRole role;
    int64_t joined_at;
};

class GroupRepository;

class GroupService {
public:
    GroupService(std::shared_ptr<odb::pgsql::database> db,
                 std::shared_ptr<im::service::user::UserService> user_service);
    ~GroupService();

    GroupResult create_group(const CreateGroupRequest& request);
    GroupResult join_group(uint64_t group_id, const std::string& user_uid,
                           int64_t now_ms = 0);
    GroupResult leave_group(uint64_t group_id, const std::string& user_uid);
    std::vector<GroupInfoDTO> list_my_groups(const std::string& user_uid);
    bool group_exists(uint64_t group_id);
    std::vector<MemberInfoDTO> list_members(uint64_t group_id,
                                            const std::string& caller_uid);

private:
    std::string resolve_nickname(const std::string& uid);

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::unique_ptr<GroupRepository> repo_;
};

} // namespace group
} // namespace service
} // namespace im

#endif // IM_SERVICE_GROUP_GROUP_SERVICE_HPP
