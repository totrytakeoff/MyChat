#ifndef IM_SERVICE_GROUP_GROUP_REPOSITORY_HPP
#define IM_SERVICE_GROUP_GROUP_REPOSITORY_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odb {
namespace pgsql {
class database;
}
}

namespace im {
namespace service {
namespace group {

enum class GroupRole : int;
class Group;
class GroupMember;

class GroupRepository {
public:
    explicit GroupRepository(std::shared_ptr<odb::pgsql::database> db);

    // Group operations
    bool create_group(Group& g);
    bool create_group_with_owner(Group& g, GroupMember& owner);
    std::optional<Group> find_group_by_id(uint64_t group_id);
    std::vector<Group> find_groups_by_user(const std::string& user_uid);

    // Member operations
    bool add_member(GroupMember m);
    bool remove_member(uint64_t group_id, const std::string& user_uid);
    bool update_member_role(uint64_t group_id, const std::string& user_uid,
                            GroupRole role);
    std::vector<GroupMember> find_members(uint64_t group_id);
    std::optional<GroupMember> find_member(uint64_t group_id,
                                           const std::string& user_uid);

private:
    std::shared_ptr<odb::pgsql::database> db_;
};

} // namespace group
} // namespace service
} // namespace im

#endif // IM_SERVICE_GROUP_GROUP_REPOSITORY_HPP
