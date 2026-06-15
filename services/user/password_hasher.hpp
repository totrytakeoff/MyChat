#ifndef IM_SERVICE_USER_PASSWORD_HASHER_HPP
#define IM_SERVICE_USER_PASSWORD_HASHER_HPP

#include <string>

namespace im {
namespace service {
namespace user {

class PasswordHasher {
public:
    explicit PasswordHasher(int iterations = 100000);

    std::string hash(const std::string& password) const;
    bool verify(const std::string& password, const std::string& stored_hash) const;


/*
 *  默认的 PBKDF2 加密迭代次数,
 *  2026当下100000轮基本合格,
 *  300000+更符合现代标准
*/
    static constexpr int kDefaultIterations = 100000;

private:
    int iterations_;
};

} // namespace user
} // namespace service
} // namespace im

#endif // IM_SERVICE_USER_PASSWORD_HASHER_HPP
