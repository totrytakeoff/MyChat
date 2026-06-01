#include "../../common/database/redis/redis_mgr.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace {

im::db::RedisConfig test_config() {
    im::db::RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "mychat-dev-pass";
    config.db = 15;
    config.connect_timeout = 1000;
    config.socket_timeout = 1000;
    return config;
}

std::string key(const std::string& suffix) {
    return "mychat:test:redis:" + suffix;
}

class RedisHiredisTest : public ::testing::Test {
protected:
    void SetUp() override {
        im::utils::LogManager::SetLogToConsole("redis_manager");
        auto& manager = im::db::redis_manager();
        manager.shutdown();
        ASSERT_TRUE(manager.initialize(test_config()))
                << "Start Redis with `docker compose up -d redis` before running this test.";
        cleanup();
    }

    void TearDown() override {
        cleanup();
        im::db::redis_manager().shutdown();
    }

    void cleanup() {
        auto& manager = im::db::redis_manager();
        manager.safe_execute(
                [](auto& redis) {
                    redis.del(key("hash"));
                    redis.del(key("set"));
                    redis.del(key("expire"));
                    return true;
                },
                false);
    }
};

}  // namespace

TEST_F(RedisHiredisTest, PingAndHealthCheck) {
    EXPECT_TRUE(im::db::redis_manager().is_healthy());
    EXPECT_EQ(im::db::redis_manager().execute([](auto& redis) { return redis.ping(); }), "PONG");
}

TEST_F(RedisHiredisTest, HashOperations) {
    auto& manager = im::db::redis_manager();

    manager.execute([](auto& redis) {
        redis.hset(key("hash"), "name", "mychat");
        redis.hset(key("hash"), "stage", "phase1");
        return true;
    });

    auto name = manager.execute([](auto& redis) { return redis.hget(key("hash"), "name"); });
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "mychat");

    std::unordered_map<std::string, std::string> values;
    manager.execute([&](auto& redis) {
        redis.hgetall(key("hash"), std::inserter(values, values.begin()));
        return true;
    });
    EXPECT_EQ(values["name"], "mychat");
    EXPECT_EQ(values["stage"], "phase1");

    manager.execute([](auto& redis) {
        redis.hdel(key("hash"), "stage");
        return true;
    });
    auto removed = manager.execute([](auto& redis) { return redis.hget(key("hash"), "stage"); });
    EXPECT_FALSE(removed.has_value());
}

TEST_F(RedisHiredisTest, SetOperations) {
    auto& manager = im::db::redis_manager();

    manager.execute([](auto& redis) {
        redis.sadd(key("set"), "web");
        redis.sadd(key("set"), "desktop");
        return true;
    });

    EXPECT_TRUE(manager.execute([](auto& redis) { return redis.sismember(key("set"), "web"); }));
    EXPECT_EQ(manager.execute([](auto& redis) { return redis.scard(key("set")); }), 2);

    std::unordered_set<std::string> members;
    manager.execute([&](auto& redis) {
        redis.smembers(key("set"), std::inserter(members, members.begin()));
        return true;
    });
    EXPECT_TRUE(members.contains("web"));
    EXPECT_TRUE(members.contains("desktop"));

    manager.execute([](auto& redis) {
        redis.srem(key("set"), "web");
        return true;
    });
    EXPECT_FALSE(manager.execute([](auto& redis) { return redis.sismember(key("set"), "web"); }));
}

TEST_F(RedisHiredisTest, ExpireRemovesKey) {
    auto& manager = im::db::redis_manager();

    manager.execute([](auto& redis) {
        redis.hset(key("expire"), "field", "value");
        redis.expire(key("expire"), 1);
        return true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    auto value = manager.execute([](auto& redis) { return redis.hget(key("expire"), "field"); });
    EXPECT_FALSE(value.has_value());
}

TEST_F(RedisHiredisTest, SafeExecuteReturnsDefaultOnFailure) {
    auto& manager = im::db::redis_manager();
    manager.shutdown();

    auto result = manager.safe_execute([](auto&) { return 42; }, -1);
    EXPECT_EQ(result, -1);
}
