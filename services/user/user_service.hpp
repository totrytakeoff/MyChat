#ifndef IM_SERVICE_USER_USER_SERVICE_HPP
#define IM_SERVICE_USER_USER_SERVICE_HPP

#include <memory>
#include <optional>
#include <string>

namespace odb {
namespace pgsql {
class database;
}
}

namespace im {
namespace service {
namespace user {

enum class Gender;

struct UserProfile {
    std::string uid;
    std::string account;
    std::string nickname;
    std::string avatar;
    Gender gender;
    std::string signature;
    int64_t create_time;
    int64_t last_login;
    bool online;
};

struct RegisterRequest {
    std::string account;
    std::string password;
    std::string nickname;
    int64_t now_ms = 0;
};

struct RegisterResult {
    bool ok = false;
    std::string error_code;
    std::string message;
    UserProfile profile;
};

struct LoginResult {
    bool ok = false;
    std::string error_code;
    std::string message;
    UserProfile profile;
};

class UserRepository;
class PasswordHasher;

class UserService {
public:
    UserService(std::shared_ptr<odb::pgsql::database> db,
                std::unique_ptr<PasswordHasher> hasher);
    ~UserService();

    RegisterResult register_user(const RegisterRequest& request);
    LoginResult login_by_account(const std::string& account,
                                 const std::string& password,
                                 int64_t now_ms);
    std::optional<UserProfile> get_profile_by_uid(const std::string& uid);
    std::optional<UserProfile> get_profile_by_account(const std::string& account);

private:
    UserProfile to_profile(const class User& user) const;

    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<PasswordHasher> hasher_;
    std::unique_ptr<UserRepository> repo_;
};

} // namespace user
} // namespace service
} // namespace im

#endif // IM_SERVICE_USER_USER_SERVICE_HPP
