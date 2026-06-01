#include "redis_mgr.hpp"

#include <cstdarg>

namespace im::db {

RedisConfig RedisConfig::from_file(const std::string& config_path) {
    im::utils::ConfigManager cfg(config_path);
    RedisConfig config;
    config.host = cfg.get<std::string>("redis.host", "127.0.0.1");
    config.port = cfg.get<int>("redis.port", 6379);
    config.password = cfg.get<std::string>("redis.password", "");
    config.db = cfg.get<int>("redis.db", 0);
    config.pool_size = cfg.get<int>("redis.pool_size", 1);
    config.connect_timeout = cfg.get<int>("redis.connect_timeout", 1000);
    config.socket_timeout = cfg.get<int>("redis.socket_timeout", 1000);
    return config;
}

RedisClient::RedisClient(RedisConfig config) : config_(std::move(config)) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    connect_locked();
}

RedisClient::~RedisClient() {
    std::lock_guard<std::mutex> lock(context_mutex_);
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
}

void RedisClient::connect_locked() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }

    timeval timeout{};
    timeout.tv_sec = config_.connect_timeout / 1000;
    timeout.tv_usec = (config_.connect_timeout % 1000) * 1000;

    context_ = redisConnectWithTimeout(config_.host.c_str(), config_.port, timeout);
    if (!context_ || context_->err) {
        std::string err = context_ ? context_->errstr : "allocation failed";
        throw std::runtime_error("Failed to connect to Redis: " + err);
    }

    timeval socket_timeout{};
    socket_timeout.tv_sec = config_.socket_timeout / 1000;
    socket_timeout.tv_usec = (config_.socket_timeout % 1000) * 1000;
    redisSetTimeout(context_, socket_timeout);

    if (!config_.password.empty()) {
        ReplyPtr reply(static_cast<redisReply*>(
                               redisCommand(context_, "AUTH %b", config_.password.data(),
                                            config_.password.size())),
                       freeReplyObject);
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            throw std::runtime_error("Redis AUTH failed");
        }
    }

    if (config_.db != 0) {
        ReplyPtr reply(static_cast<redisReply*>(redisCommand(context_, "SELECT %d", config_.db)),
                       freeReplyObject);
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            throw std::runtime_error("Redis SELECT failed");
        }
    }
}

RedisClient::ReplyPtr RedisClient::command(const char* format, ...) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    if (!context_ || context_->err) {
        connect_locked();
    }

    va_list args;
    va_start(args, format);
    auto* raw = static_cast<redisReply*>(redisvCommand(context_, format, args));
    va_end(args);

    if (!raw) {
        std::string err = context_ ? context_->errstr : "unknown";
        connect_locked();
        throw std::runtime_error("Redis command failed: " + err);
    }

    ReplyPtr reply(raw, freeReplyObject);
    if (reply->type == REDIS_REPLY_ERROR) {
        throw std::runtime_error("Redis error: " + reply_string(reply.get()));
    }
    return reply;
}

std::string RedisClient::reply_string(const redisReply* reply) {
    if (!reply || !reply->str) {
        return {};
    }
    return std::string(reply->str, reply->len);
}

std::string RedisClient::ping() {
    return reply_string(command("PING").get());
}

void RedisClient::hset(const std::string& key, const std::string& field, const std::string& value) {
    command("HSET %b %b %b", key.data(), key.size(), field.data(), field.size(), value.data(),
            value.size());
}

std::optional<std::string> RedisClient::hget(const std::string& key, const std::string& field) {
    auto reply = command("HGET %b %b", key.data(), key.size(), field.data(), field.size());
    if (reply->type == REDIS_REPLY_NIL) {
        return std::nullopt;
    }
    return reply_string(reply.get());
}

void RedisClient::hdel(const std::string& key, const std::string& field) {
    command("HDEL %b %b", key.data(), key.size(), field.data(), field.size());
}

void RedisClient::sadd(const std::string& key, const std::string& member) {
    command("SADD %b %b", key.data(), key.size(), member.data(), member.size());
}

void RedisClient::srem(const std::string& key, const std::string& member) {
    command("SREM %b %b", key.data(), key.size(), member.data(), member.size());
}

bool RedisClient::sismember(const std::string& key, const std::string& member) {
    auto reply = command("SISMEMBER %b %b", key.data(), key.size(), member.data(),
                         member.size());
    return reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
}

int64_t RedisClient::scard(const std::string& key) {
    auto reply = command("SCARD %b", key.data(), key.size());
    return reply->type == REDIS_REPLY_INTEGER ? reply->integer : 0;
}

void RedisClient::set(const std::string& key, const std::string& value) {
    command("SET %b %b", key.data(), key.size(), value.data(), value.size());
}

std::optional<std::string> RedisClient::get(const std::string& key) {
    auto reply = command("GET %b", key.data(), key.size());
    if (reply->type == REDIS_REPLY_NIL) {
        return std::nullopt;
    }
    return reply_string(reply.get());
}

bool RedisClient::exists(const std::string& key) {
    auto reply = command("EXISTS %b", key.data(), key.size());
    return reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
}

int64_t RedisClient::ttl(const std::string& key) {
    auto reply = command("TTL %b", key.data(), key.size());
    return reply->type == REDIS_REPLY_INTEGER ? reply->integer : -2;
}

void RedisClient::del(const std::string& key) {
    command("DEL %b", key.data(), key.size());
}

std::vector<std::string> RedisClient::keys(const std::string& pattern) {
    auto reply = command("KEYS %b", pattern.data(), pattern.size());
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        result.reserve(reply->elements);
        for (size_t i = 0; i < reply->elements; ++i) {
            result.push_back(reply_string(reply->element[i]));
        }
    }
    return result;
}

void RedisClient::expire(const std::string& key, int seconds) {
    command("EXPIRE %b %d", key.data(), key.size(), seconds);
}

bool RedisManager::initialize(const std::string& config_path) {
    try {
        return initialize(RedisConfig::from_file(config_path));
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("redis_manager")
                ->error("Failed to load Redis config {}: {}", config_path, e.what());
        return false;
    }
}

bool RedisManager::initialize(const RedisConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        config_ = config;
        redis_ = std::make_shared<RedisClient>(config_);
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        initialized_ = false;
        redis_.reset();
        im::utils::LogManager::GetLogger("redis_manager")
                ->error("Failed to initialize Redis manager: {}", e.what());
        return false;
    }
}

RedisManager::RedisConnection RedisManager::get_connection() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !redis_) {
        throw std::runtime_error("Redis manager is not initialized");
    }
    return RedisConnection(redis_);
}

RedisManager::PoolStats RedisManager::get_pool_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_ ? PoolStats{1, 1, 0} : PoolStats{};
}

bool RedisManager::is_healthy() const {
    std::shared_ptr<RedisClient> redis;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        redis = redis_;
    }
    if (!redis) {
        return false;
    }

    try {
        return redis->ping() == "PONG";
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("redis_manager")
                ->error("Redis health check failed: {}", e.what());
        return false;
    }
}

void RedisManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    redis_.reset();
    initialized_ = false;
}

}  // namespace im::db
