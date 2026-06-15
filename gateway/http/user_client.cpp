#include "user_client.hpp"

#include <stdexcept>
#include <utility>

namespace im::gateway {

LocalUserClient::LocalUserClient(
    std::shared_ptr<im::service::user::UserService> user_service)
    : user_service_(std::move(user_service)) {
    if (!user_service_) {
        throw std::invalid_argument("LocalUserClient requires UserService");
    }
}

im::service::user::RegisterResult LocalUserClient::register_user(
    const im::service::user::RegisterRequest& request) {
    return user_service_->register_user(request);
}

im::service::user::LoginResult LocalUserClient::login_by_account(
    const std::string& account,
    const std::string& password,
    int64_t now_ms) {
    return user_service_->login_by_account(account, password, now_ms);
}

std::optional<im::service::user::UserProfile> LocalUserClient::get_profile_by_uid(
    const std::string& uid) {
    return user_service_->get_profile_by_uid(uid);
}

std::optional<im::service::user::UserProfile> LocalUserClient::get_profile_by_account(
    const std::string& account) {
    return user_service_->get_profile_by_account(account);
}

std::vector<im::service::user::UserProfile> LocalUserClient::search_profiles(
    const std::string& keyword,
    std::size_t limit) {
    return user_service_->search_profiles(keyword, limit);
}

im::service::user::UpdateProfileResult LocalUserClient::update_profile(
    const im::service::user::UpdateProfileRequest& request) {
    return user_service_->update_profile(request);
}

}  // namespace im::gateway
