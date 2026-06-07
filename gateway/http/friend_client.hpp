#ifndef GATEWAY_HTTP_FRIEND_CLIENT_HPP
#define GATEWAY_HTTP_FRIEND_CLIENT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../services/friend/friend_service.hpp"

namespace im::gateway {

class FriendClient {
public:
    virtual ~FriendClient() = default;

    virtual im::service::friend_::FriendResult send_request(
        const im::service::friend_::FriendRequest& request) = 0;

    virtual im::service::friend_::FriendResult respond_to_request(
        uint64_t friend_id,
        const std::string& uid,
        bool accept) = 0;

    virtual std::vector<im::service::friend_::FriendInfoDTO> get_friends(
        const std::string& uid) = 0;

    virtual std::vector<im::service::friend_::FriendInfoDTO> get_pending_requests(
        const std::string& uid) = 0;
};

class LocalFriendClient final : public FriendClient {
public:
    explicit LocalFriendClient(
        std::shared_ptr<im::service::friend_::FriendService> friend_service);

    im::service::friend_::FriendResult send_request(
        const im::service::friend_::FriendRequest& request) override;

    im::service::friend_::FriendResult respond_to_request(
        uint64_t friend_id,
        const std::string& uid,
        bool accept) override;

    std::vector<im::service::friend_::FriendInfoDTO> get_friends(
        const std::string& uid) override;

    std::vector<im::service::friend_::FriendInfoDTO> get_pending_requests(
        const std::string& uid) override;

private:
    std::shared_ptr<im::service::friend_::FriendService> friend_service_;
};

}  // namespace im::gateway

#endif  // GATEWAY_HTTP_FRIEND_CLIENT_HPP
