#ifndef IM_SERVICE_FRIEND_FRIEND_SERVICE_HPP
#define IM_SERVICE_FRIEND_FRIEND_SERVICE_HPP

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
namespace user {
class UserService;
}

namespace friend_ {

enum class FriendStatus : int;

struct FriendRequest {
    std::string requester_uid;
    std::string target_uid;
    int64_t now_ms = 0;
};

struct FriendResult {
    bool ok = false;
    std::string error_code;
    std::string message;
};

struct FriendInfoDTO {
    uint64_t friend_id;
    std::string friend_uid;
    std::string nickname;
    FriendStatus status;
    int64_t created_at;
};

class FriendRepository;

class FriendService {
public:
    FriendService(std::shared_ptr<odb::pgsql::database> db,
                  std::shared_ptr<im::service::user::UserService> user_service);
    ~FriendService();

    FriendResult send_request(const FriendRequest& request);
    FriendResult respond_to_request(uint64_t friend_id, const std::string& uid,
                                    bool accept);
    std::vector<FriendInfoDTO> get_friends(const std::string& uid);
    std::vector<FriendInfoDTO> get_pending_requests(const std::string& uid);

private:
    std::string resolve_nickname(const std::string& uid);
    FriendInfoDTO to_friend_info(const class Friend& f, const std::string& uid);

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::unique_ptr<FriendRepository> repo_;
};

} // namespace friend_
} // namespace service
} // namespace im

#endif // IM_SERVICE_FRIEND_FRIEND_SERVICE_HPP
