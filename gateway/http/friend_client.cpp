#include "friend_client.hpp"

#include <utility>

namespace im::gateway {

LocalFriendClient::LocalFriendClient(
    std::shared_ptr<im::service::friend_::FriendService> friend_service)
    : friend_service_(std::move(friend_service)) {}

im::service::friend_::FriendResult LocalFriendClient::send_request(
    const im::service::friend_::FriendRequest& request) {
    return friend_service_->send_request(request);
}

im::service::friend_::FriendResult LocalFriendClient::respond_to_request(
    uint64_t friend_id,
    const std::string& uid,
    bool accept) {
    return friend_service_->respond_to_request(friend_id, uid, accept);
}

std::vector<im::service::friend_::FriendInfoDTO> LocalFriendClient::get_friends(
    const std::string& uid) {
    return friend_service_->get_friends(uid);
}

std::vector<im::service::friend_::FriendInfoDTO> LocalFriendClient::get_pending_requests(
    const std::string& uid) {
    return friend_service_->get_pending_requests(uid);
}

}  // namespace im::gateway
