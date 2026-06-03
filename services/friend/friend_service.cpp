#include "friend_service.hpp"
#include "friend_repository.hpp"

#include <utility>
#include <chrono>

#include <odb/pgsql/database.hxx>

#include <friend.hpp>
#include "../user/user_service.hpp"

namespace im {
namespace service {
namespace friend_ {

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // anonymous namespace

FriendService::FriendService(
    std::shared_ptr<odb::pgsql::database> db,
    std::shared_ptr<im::service::user::UserService> user_service)
    : db_(std::move(db))
    , user_service_(std::move(user_service))
    , repo_(std::make_unique<FriendRepository>(db_)) {}

FriendService::~FriendService() = default;

FriendResult FriendService::send_request(const FriendRequest& request) {
    FriendResult result;

    if (request.requester_uid.empty() || request.target_uid.empty()) {
        result.error_code = "EMPTY_UID";
        result.message = "Requester and target UIDs must not be empty";
        return result;
    }

    if (request.requester_uid == request.target_uid) {
        result.error_code = "SELF_REQUEST";
        result.message = "Cannot send friend request to yourself";
        return result;
    }

    // Verify target user exists
    if (!user_service_->get_profile_by_uid(request.target_uid).has_value()) {
        result.error_code = "TARGET_NOT_FOUND";
        result.message = "Target user does not exist";
        return result;
    }

    // Check if a relationship already exists in either direction
    auto existing = repo_->find_relationship(request.requester_uid, request.target_uid);
    if (existing.has_value()) {
        result.error_code = "ALREADY_EXISTS";
        result.message = "A relationship between these users already exists";
        return result;
    }

    int64_t t = request.now_ms > 0 ? request.now_ms : now_ms();

    Friend f(request.requester_uid, request.target_uid,
             FriendStatus::PENDING, t);

    if (!repo_->create(f)) {
        result.error_code = "PERSIST_FAILED";
        result.message = "Failed to persist friend request";
        return result;
    }

    result.ok = true;
    result.message = "Friend request sent";
    return result;
}

FriendResult FriendService::respond_to_request(
    uint64_t friend_id, const std::string& uid, bool accept)
{
    FriendResult result;

    auto opt_f = repo_->find_by_id(friend_id);
    if (!opt_f.has_value()) {
        result.error_code = "NOT_FOUND";
        result.message = "Friend request not found";
        return result;
    }

    const Friend& f = opt_f.value();

    // Only the target user can respond
    if (f.target_uid() != uid) {
        result.error_code = "FORBIDDEN";
        result.message = "Only the target user can respond to this request";
        return result;
    }

    if (f.status() != FriendStatus::PENDING) {
        result.error_code = "NOT_PENDING";
        result.message = "Friend request is not in pending status";
        return result;
    }

    FriendStatus new_status = accept ? FriendStatus::ACCEPTED : FriendStatus::REJECTED;

    if (!repo_->update_status(friend_id, new_status)) {
        result.error_code = "UPDATE_FAILED";
        result.message = "Failed to update friend request status";
        return result;
    }

    result.ok = true;
    result.message = accept ? "Friend request accepted" : "Friend request rejected";
    return result;
}

std::vector<FriendInfoDTO> FriendService::get_friends(const std::string& uid) {
    std::vector<FriendInfoDTO> result;
    auto friends = repo_->find_friends(uid);
    result.reserve(friends.size());
    for (const auto& f : friends) {
        result.push_back(to_friend_info(f, uid));
    }
    return result;
}

std::vector<FriendInfoDTO> FriendService::get_pending_requests(const std::string& uid) {
    std::vector<FriendInfoDTO> result;
    auto pending = repo_->find_pending(uid);
    result.reserve(pending.size());
    for (const auto& f : pending) {
        result.push_back(to_friend_info(f, uid));
    }
    return result;
}

std::string FriendService::resolve_nickname(const std::string& uid) {
    if (auto profile = user_service_->get_profile_by_uid(uid)) {
        return profile->nickname;
    }
    return uid;
}

FriendInfoDTO FriendService::to_friend_info(
    const Friend& f, const std::string& uid)
{
    FriendInfoDTO dto;
    dto.friend_id = f.friend_id();
    dto.status = f.status();
    dto.created_at = f.created_at();

    // Determine the other user's UID
    std::string other_uid = (f.requester_uid() == uid)
        ? f.target_uid()
        : f.requester_uid();
    dto.friend_uid = other_uid;
    dto.nickname = resolve_nickname(other_uid);

    return dto;
}

} // namespace friend_
} // namespace service
} // namespace im
