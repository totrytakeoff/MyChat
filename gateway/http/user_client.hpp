#ifndef GATEWAY_HTTP_USER_CLIENT_HPP
#define GATEWAY_HTTP_USER_CLIENT_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../services/user/user_service.hpp"

namespace im::gateway {

class UserClient {
public:
    virtual ~UserClient() = default;

    virtual im::service::user::RegisterResult register_user(
        const im::service::user::RegisterRequest& request) = 0;

    virtual im::service::user::LoginResult login_by_account(
        const std::string& account,
        const std::string& password,
        int64_t now_ms) = 0;

    virtual std::optional<im::service::user::UserProfile> get_profile_by_uid(
        const std::string& uid) = 0;

    virtual std::optional<im::service::user::UserProfile> get_profile_by_account(
        const std::string& account) = 0;

    virtual std::vector<im::service::user::UserProfile> search_profiles(
        const std::string& keyword,
        std::size_t limit) = 0;

    virtual im::service::user::UpdateProfileResult update_profile(
        const im::service::user::UpdateProfileRequest& request) = 0;
};

class LocalUserClient final : public UserClient {
public:
    explicit LocalUserClient(std::shared_ptr<im::service::user::UserService> user_service);

    im::service::user::RegisterResult register_user(
        const im::service::user::RegisterRequest& request) override;

    im::service::user::LoginResult login_by_account(
        const std::string& account,
        const std::string& password,
        int64_t now_ms) override;

    std::optional<im::service::user::UserProfile> get_profile_by_uid(
        const std::string& uid) override;

    std::optional<im::service::user::UserProfile> get_profile_by_account(
        const std::string& account) override;

    std::vector<im::service::user::UserProfile> search_profiles(
        const std::string& keyword,
        std::size_t limit) override;

    im::service::user::UpdateProfileResult update_profile(
        const im::service::user::UpdateProfileRequest& request) override;

private:
    std::shared_ptr<im::service::user::UserService> user_service_;
};

}  // namespace im::gateway

#endif  // GATEWAY_HTTP_USER_CLIENT_HPP
