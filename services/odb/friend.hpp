#ifndef IM_SERVICE_FRIEND_FRIEND_HPP
#define IM_SERVICE_FRIEND_FRIEND_HPP

#include <odb/core.hxx>
#include <string>
#include <cstdint>

namespace im {
namespace service {
namespace friend_ {

enum class FriendStatus : int {
    PENDING  = 0,
    ACCEPTED = 1,
    REJECTED = 2,
    BLOCKED  = 3
};

#pragma db object table("im_friends")
class Friend {
public:
    Friend() : status_(FriendStatus::PENDING), created_at_(0) {}

    Friend(const std::string& requester_uid,
           const std::string& target_uid,
           FriendStatus status = FriendStatus::PENDING,
           int64_t created_at = 0)
        : requester_uid_(requester_uid), target_uid_(target_uid),
          status_(status), created_at_(created_at) {}

    uint64_t friend_id() const { return friend_id_; }
    void friend_id(uint64_t v) { friend_id_ = v; }

    const std::string& requester_uid() const { return requester_uid_; }
    void requester_uid(const std::string& v) { requester_uid_ = v; }

    const std::string& target_uid() const { return target_uid_; }
    void target_uid(const std::string& v) { target_uid_ = v; }

    FriendStatus status() const { return status_; }
    void status(FriendStatus v) { status_ = v; }

    int64_t created_at() const { return created_at_; }
    void created_at(int64_t v) { created_at_ = v; }

private:
    friend class odb::access;

    #pragma db id auto
    uint64_t friend_id_;

    #pragma db not_null
    std::string requester_uid_;

    #pragma db not_null
    std::string target_uid_;

    #pragma db value_not_null
    FriendStatus status_;

    int64_t created_at_;

    #pragma db index("im_friends_pair_i") unique members(requester_uid_, target_uid_)
};

} // namespace friend_
} // namespace service
} // namespace im

#endif // IM_SERVICE_FRIEND_FRIEND_HPP
