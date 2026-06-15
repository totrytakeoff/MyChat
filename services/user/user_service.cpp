#include "user_service.hpp"
#include "password_hasher.hpp"
#include "user_repository.hpp"

#include <memory>

#include <cstdio>
#include <stdexcept>
#include <utility>

#include <openssl/rand.h>

#include <user.hpp>

namespace im {
namespace service {
namespace user {

namespace {

std::string GenerateUid() {
    unsigned char bytes[16];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        throw std::runtime_error("failed to generate random UUID bytes");
    }
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    char hex[37];
    std::snprintf(hex, sizeof(hex),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11],
        bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(hex);
}

} // anonymous namespace

UserService::UserService(std::shared_ptr<odb::pgsql::database> db,
                         std::unique_ptr<PasswordHasher> hasher)
    : db_(std::move(db))
    , hasher_(std::move(hasher))
    , repo_(std::make_unique<UserRepository>(db_)) {}

UserService::~UserService() = default;

RegisterResult UserService::register_user(const RegisterRequest& request) {
    RegisterResult result;

    if (request.account.empty()) {
        result.error_code = "EMPTY_ACCOUNT";
        result.message = "Account must not be empty";
        return result;
    }

    if (request.password.empty()) {
        result.error_code = "EMPTY_PASSWORD";
        result.message = "Password must not be empty";
        return result;
    }

    if (repo_->account_exists(request.account)) {
        result.error_code = "DUPLICATE_ACCOUNT";
        result.message = "Account already exists";
        return result;
    }

    std::string uid = GenerateUid();
    std::string password_hash = hasher_->hash(request.password);
    std::string nickname = request.nickname.empty()
        ? request.account
        : request.nickname;

    User user(uid, request.account, password_hash, nickname,
              "", Gender::UNKNOWN, "",
              request.now_ms, request.now_ms, false);

    if (!repo_->create(user)) {
        result.error_code = "PERSIST_FAILED";
        result.message = "Failed to persist user";
        return result;
    }

    result.ok = true;
    result.profile = to_profile(user);
    return result;
}

LoginResult UserService::login_by_account(const std::string& account,
                                           const std::string& password,
                                           int64_t now_ms) {
    LoginResult result;

    auto opt_user = repo_->find_by_account(account);
    if (!opt_user.has_value()) {
        result.error_code = "INVALID_ACCOUNT";
        result.message = "Account not found";
        return result;
    }

    User user = std::move(opt_user.value());
    if (!hasher_->verify(password, user.password_hash())) {
        result.error_code = "WRONG_PASSWORD";
        result.message = "Incorrect password";
        return result;
    }

    auto prev_last_login = user.last_login();
    user.last_login(now_ms);
    if (!repo_->update_last_login(user.uid(), now_ms)) {
        user.last_login(prev_last_login);
    }

    result.ok = true;
    result.profile = to_profile(user);
    return result;
}

std::optional<UserProfile> UserService::get_profile_by_uid(const std::string& uid) {
    auto opt_user = repo_->find_by_uid(uid);
    if (!opt_user.has_value()) {
        return std::nullopt;
    }
    return to_profile(opt_user.value());
}

std::optional<UserProfile> UserService::get_profile_by_account(const std::string& account) {
    auto opt_user = repo_->find_by_account(account);
    if (!opt_user.has_value()) {
        return std::nullopt;
    }
    return to_profile(opt_user.value());
}

std::vector<UserProfile> UserService::search_profiles(const std::string& keyword,
                                                      std::size_t limit) {
    std::vector<UserProfile> profiles;
    if (keyword.empty() || limit == 0) {
        return profiles;
    }

    if (auto by_uid = get_profile_by_uid(keyword)) {
        profiles.push_back(*by_uid);
        return profiles;
    }

    auto users = repo_->search_by_account_or_nickname(keyword, limit);
    profiles.reserve(users.size());
    for (const auto& user : users) {
        profiles.push_back(to_profile(user));
    }
    return profiles;
}

UpdateProfileResult UserService::update_profile(const UpdateProfileRequest& request) {
    UpdateProfileResult result;
    if (request.uid.empty()) {
        result.error_code = "EMPTY_UID";
        result.message = "User uid must not be empty";
        return result;
    }
    if (request.nickname.empty()) {
        result.error_code = "EMPTY_NICKNAME";
        result.message = "Nickname must not be empty";
        return result;
    }

    auto updated = repo_->update_profile(
        request.uid,
        request.nickname,
        request.avatar,
        request.gender,
        request.signature);
    if (!updated.has_value()) {
        result.error_code = "USER_NOT_FOUND";
        result.message = "User profile not found";
        return result;
    }

    result.ok = true;
    result.profile = to_profile(*updated);
    return result;
}

UserProfile UserService::to_profile(const User& user) const {
    UserProfile profile;
    profile.uid = user.uid();
    profile.account = user.account();
    profile.nickname = user.nickname();
    profile.avatar = user.avatar();
    profile.gender = user.gender();
    profile.signature = user.signature();
    profile.create_time = user.create_time();
    profile.last_login = user.last_login();
    profile.online = user.online();
    return profile;
}

} // namespace user
} // namespace service
} // namespace im
