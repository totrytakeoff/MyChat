#ifndef IM_SERVICE_FRIEND_FRIEND_REPOSITORY_HPP
#define IM_SERVICE_FRIEND_FRIEND_REPOSITORY_HPP

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
namespace friend_ {

enum class FriendStatus : int;
class Friend;

class FriendRepository {
public:
    explicit FriendRepository(std::shared_ptr<odb::pgsql::database> db);

    bool create(Friend f);
    std::optional<Friend> find_by_id(uint64_t friend_id);
    std::optional<Friend> find_by_pair(const std::string& requester_uid,
                                       const std::string& target_uid);
    std::optional<Friend> find_relationship(const std::string& user_a,
                                            const std::string& user_b);
    std::vector<Friend> find_friends(const std::string& uid);
    std::vector<Friend> find_pending(const std::string& uid);
    bool update_status(uint64_t friend_id, FriendStatus status);
    bool relationship_exists(const std::string& user_a, const std::string& user_b);

private:
    std::shared_ptr<odb::pgsql::database> db_;
};

} // namespace friend_
} // namespace service
} // namespace im

#endif // IM_SERVICE_FRIEND_FRIEND_REPOSITORY_HPP
