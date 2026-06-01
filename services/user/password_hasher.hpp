#ifndef IM_SERVICE_USER_PASSWORD_HASHER_HPP
#define IM_SERVICE_USER_PASSWORD_HASHER_HPP

#include <string>

namespace im {
namespace service {
namespace user {

class PasswordHasher {
public:
    explicit PasswordHasher(int iterations = 10000);

    std::string hash(const std::string& password) const;
    bool verify(const std::string& password, const std::string& stored_hash) const;

    static constexpr int kDefaultIterations = 10000;

private:
    int iterations_;
};

} // namespace user
} // namespace service
} // namespace im

#endif // IM_SERVICE_USER_PASSWORD_HASHER_HPP
