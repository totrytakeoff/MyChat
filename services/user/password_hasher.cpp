#include "password_hasher.hpp"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace im {
namespace service {
namespace user {

namespace {

constexpr int kSaltLength = 16;
constexpr int kHashLength = 32;
constexpr char kDelimiter = '$';

std::string ToHex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

bool FromHex(const std::string& hex, unsigned char* out, size_t out_len) {
    if (hex.size() != out_len * 2) return false;
    for (size_t i = 0; i < out_len; ++i) {
        auto byte_str = hex.substr(i * 2, 2);
        char* end = nullptr;
        long val = std::strtol(byte_str.c_str(), &end, 16);
        if (end != byte_str.c_str() + 2) return false;
        out[i] = static_cast<unsigned char>(val);
    }
    return true;
}

} // anonymous namespace

PasswordHasher::PasswordHasher(int iterations)
    : iterations_(iterations > 0 ? iterations : kDefaultIterations) {}

std::string PasswordHasher::hash(const std::string& password) const {
    unsigned char salt[kSaltLength];
    if (RAND_bytes(salt, kSaltLength) != 1) {
        throw std::runtime_error("failed to generate random salt");
    }

    unsigned char hash[kHashLength];
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                          salt, kSaltLength, iterations_,
                          EVP_sha256(), kHashLength, hash) != 1) {
        throw std::runtime_error("PBKDF2 hashing failed");
    }

    std::ostringstream oss;
    oss << "pbkdf2_sha256" << kDelimiter
        << iterations_ << kDelimiter
        << ToHex(salt, kSaltLength) << kDelimiter
        << ToHex(hash, kHashLength);
    return oss.str();
}

bool PasswordHasher::verify(const std::string& password,
                            const std::string& stored_hash) const {
    std::istringstream iss(stored_hash);
    std::string part;

    if (!std::getline(iss, part, kDelimiter) || part != "pbkdf2_sha256") {
        return false;
    }

    if (!std::getline(iss, part, kDelimiter)) return false;
    char* end = nullptr;
    long stored_iter = std::strtol(part.c_str(), &end, 10);
    if (end != part.c_str() + part.size()) return false;

    if (!std::getline(iss, part, kDelimiter)) return false;
    unsigned char salt[kSaltLength];
    if (!FromHex(part, salt, kSaltLength)) return false;

    if (!std::getline(iss, part, kDelimiter)) return false;
    unsigned char expected_hash[kHashLength];
    if (!FromHex(part, expected_hash, kHashLength)) return false;

    unsigned char computed_hash[kHashLength];
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                          salt, kSaltLength, static_cast<int>(stored_iter),
                          EVP_sha256(), kHashLength, computed_hash) != 1) {
        return false;
    }

    return (CRYPTO_memcmp(computed_hash, expected_hash, kHashLength) == 0);
}

} // namespace user
} // namespace service
} // namespace im
