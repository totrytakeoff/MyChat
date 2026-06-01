#ifndef REDIS_MGR_HPP
#define REDIS_MGR_HPP

#include <hiredis/hiredis.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../utils/config_mgr.hpp"
#include "../../utils/log_manager.hpp"
#include "../../utils/singleton.hpp"

namespace im::db {

struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string password;
    int db = 0;
    int pool_size = 1;
    int connect_timeout = 1000;
    int socket_timeout = 1000;

    static RedisConfig from_file(const std::string& config_path);
};

class RedisClient {
public:
    explicit RedisClient(RedisConfig config);
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    std::string ping();

    void hset(const std::string& key, const std::string& field, const std::string& value);
    std::optional<std::string> hget(const std::string& key, const std::string& field);
    template <typename OutputIt>
    void hgetall(const std::string& key, OutputIt out);
    void hdel(const std::string& key, const std::string& field);

    void sadd(const std::string& key, const std::string& member);
    void srem(const std::string& key, const std::string& member);
    bool sismember(const std::string& key, const std::string& member);
    int64_t scard(const std::string& key);
    template <typename OutputIt>
    void smembers(const std::string& key, OutputIt out);

    void del(const std::string& key);
    void expire(const std::string& key, int seconds);

private:
    using ReplyPtr = std::unique_ptr<redisReply, void (*)(void*)>;

    ReplyPtr command(const char* format, ...);
    void connect_locked();
    static std::string reply_string(const redisReply* reply);

    RedisConfig config_;
    redisContext* context_ = nullptr;
    mutable std::mutex context_mutex_;
};

class RedisManager : public im::utils::Singleton<RedisManager> {
public:
    using RedisPtr = std::shared_ptr<RedisClient>;

    class RedisConnection {
    public:
        explicit RedisConnection(RedisPtr redis = nullptr) : redis_(std::move(redis)) {}

        RedisClient* operator->() { return redis_.get(); }
        RedisClient& operator*() { return *redis_; }
        bool is_valid() const { return static_cast<bool>(redis_); }

    private:
        RedisPtr redis_;
    };

    bool initialize(const std::string& config_path);
    bool initialize(const RedisConfig& config);
    RedisConnection get_connection();

    template <typename Func>
    auto execute(Func&& func) -> decltype(func(std::declval<RedisClient&>())) {
        auto conn = get_connection();
        if (!conn.is_valid()) {
            throw std::runtime_error("Redis manager is not initialized");
        }
        return func(*conn);
    }

    template <typename Func, typename DefaultType>
    auto safe_execute(Func&& func, DefaultType&& default_value) -> decltype(func(std::declval<RedisClient&>())) {
        try {
            return execute(std::forward<Func>(func));
        } catch (const std::exception& e) {
            im::utils::LogManager::GetLogger("redis_manager")
                    ->error("Redis operation failed: {}", e.what());
            return std::forward<DefaultType>(default_value);
        }
    }

    struct PoolStats {
        size_t total_connections = 0;
        size_t available_connections = 0;
        size_t active_connections = 0;
    };

    PoolStats get_pool_stats() const;
    bool is_healthy() const;
    void shutdown();

private:
    friend class im::utils::Singleton<RedisManager>;
    RedisManager() = default;
    ~RedisManager() = default;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    RedisConfig config_;
    RedisPtr redis_;
};

inline RedisManager& redis_manager() {
    return RedisManager::GetInstance();
}

template <typename OutputIt>
void RedisClient::hgetall(const std::string& key, OutputIt out) {
    auto reply = command("HGETALL %b", key.data(), key.size());
    if (reply->type != REDIS_REPLY_ARRAY) {
        return;
    }

    for (size_t i = 0; i + 1 < reply->elements; i += 2) {
        *out++ = {reply_string(reply->element[i]), reply_string(reply->element[i + 1])};
    }
}

template <typename OutputIt>
void RedisClient::smembers(const std::string& key, OutputIt out) {
    auto reply = command("SMEMBERS %b", key.data(), key.size());
    if (reply->type != REDIS_REPLY_ARRAY) {
        return;
    }

    for (size_t i = 0; i < reply->elements; ++i) {
        *out++ = reply_string(reply->element[i]);
    }
}

}  // namespace im::db

#endif  // REDIS_MGR_HPP
