#ifndef IM_SERVICE_USER_USER_REPOSITORY_HPP
#define IM_SERVICE_USER_USER_REPOSITORY_HPP

#include <memory>
#include <optional>
#include <string>

namespace odb {
class database;
namespace pgsql {
class database;
}
}

namespace im {
namespace service {
namespace user {

class User;

class UserRepository {
public:
    explicit UserRepository(std::shared_ptr<odb::pgsql::database> db);

    bool create(const User& user);
    std::optional<User> find_by_uid(const std::string& uid);
    std::optional<User> find_by_account(const std::string& account);
    bool account_exists(const std::string& account);
    bool update_last_login(const std::string& uid, int64_t last_login);

private:
    std::shared_ptr<odb::pgsql::database> db_;
};

} // namespace user
} // namespace service
} // namespace im

#endif // IM_SERVICE_USER_USER_REPOSITORY_HPP
