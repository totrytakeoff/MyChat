#ifndef IM_SERVICE_GROUP_GROUP_HPP
#define IM_SERVICE_GROUP_GROUP_HPP

#include <odb/core.hxx>
#include <string>
#include <cstdint>

namespace im {
namespace service {
namespace group {

enum class GroupRole : int {
    OWNER  = 0,
    ADMIN  = 1,
    MEMBER = 2
};

#pragma db object table("im_groups")
class Group {
public:
    Group() : created_at_(0) {}

    Group(const std::string& name,
          const std::string& creator_uid,
          int64_t created_at = 0)
        : name_(name), creator_uid_(creator_uid), created_at_(created_at) {}

    uint64_t group_id() const { return group_id_; }
    void group_id(uint64_t v) { group_id_ = v; }

    const std::string& name() const { return name_; }
    void name(const std::string& v) { name_ = v; }

    const std::string& creator_uid() const { return creator_uid_; }
    void creator_uid(const std::string& v) { creator_uid_ = v; }

    int64_t created_at() const { return created_at_; }
    void created_at(int64_t v) { created_at_ = v; }

private:
    friend class odb::access;

    #pragma db id auto
    uint64_t group_id_;

    #pragma db not_null
    std::string name_;

    #pragma db not_null
    std::string creator_uid_;

    int64_t created_at_;
};

#pragma db object table("im_group_members")
class GroupMember {
public:
    GroupMember() : group_id_(0), role_(GroupRole::MEMBER), joined_at_(0) {}

    GroupMember(uint64_t group_id,
                const std::string& user_uid,
                GroupRole role = GroupRole::MEMBER,
                int64_t joined_at = 0)
        : group_id_(group_id), user_uid_(user_uid),
          role_(role), joined_at_(joined_at) {}

    uint64_t id() const { return id_; }
    void id(uint64_t v) { id_ = v; }

    uint64_t group_id() const { return group_id_; }
    void group_id(uint64_t v) { group_id_ = v; }

    const std::string& user_uid() const { return user_uid_; }
    void user_uid(const std::string& v) { user_uid_ = v; }

    GroupRole role() const { return role_; }
    void role(GroupRole v) { role_ = v; }

    int64_t joined_at() const { return joined_at_; }
    void joined_at(int64_t v) { joined_at_ = v; }

private:
    friend class odb::access;

    #pragma db id auto
    uint64_t id_;

    #pragma db not_null
    uint64_t group_id_;

    #pragma db not_null
    std::string user_uid_;

    #pragma db value_not_null
    GroupRole role_;

    int64_t joined_at_;

    #pragma db index("im_group_members_pair_i") unique members(group_id_, user_uid_)
};

} // namespace group
} // namespace service
} // namespace im

#endif // IM_SERVICE_GROUP_GROUP_HPP
