#ifndef IM_SERVICE_USER_USER_REPOSITORY_HPP
#define IM_SERVICE_USER_USER_REPOSITORY_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
enum class Gender;

class UserRepository {
public:
    explicit UserRepository(std::shared_ptr<odb::pgsql::database> db);

    bool create(const User& user);
    std::optional<User> find_by_uid(const std::string& uid);
    std::optional<User> find_by_account(const std::string& account);
    std::vector<User> search_by_account_or_nickname(const std::string& keyword,
                                                    std::size_t limit);
    bool account_exists(const std::string& account);
    bool update_last_login(const std::string& uid, int64_t last_login);
    std::optional<User> update_profile(const std::string& uid,
                                       const std::string& nickname,
                                       const std::string& avatar,
                                       Gender gender,
                                       const std::string& signature);

private:
    std::shared_ptr<odb::pgsql::database> db_;
};

} // namespace user
} // namespace service
} // namespace im

#endif // IM_SERVICE_USER_USER_REPOSITORY_HPP
