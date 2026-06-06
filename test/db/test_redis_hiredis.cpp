#include "../../common/database/redis/redis_mgr.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
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
    config.pool_size = 3;
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
                    redis.del(key("pool-1"));
                    redis.del(key("pool-2"));
                    redis.del(key("pool-3"));
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

TEST_F(RedisHiredisTest, PoolStatsTrackBorrowAndReturn) {
    auto& manager = im::db::redis_manager();
    auto initial = manager.get_pool_stats();
    EXPECT_EQ(initial.total_connections, 3u);
    EXPECT_EQ(initial.available_connections, 3u);
    EXPECT_EQ(initial.active_connections, 0u);

    {
        auto conn1 = manager.get_connection();
        auto conn2 = manager.get_connection();
        ASSERT_TRUE(conn1.is_valid());
        ASSERT_TRUE(conn2.is_valid());

        auto borrowed = manager.get_pool_stats();
        EXPECT_EQ(borrowed.total_connections, 3u);
        EXPECT_EQ(borrowed.available_connections, 1u);
        EXPECT_EQ(borrowed.active_connections, 2u);
        EXPECT_EQ(conn1->ping(), "PONG");
        EXPECT_EQ(conn2->ping(), "PONG");
    }

    auto returned = manager.get_pool_stats();
    EXPECT_EQ(returned.total_connections, 3u);
    EXPECT_EQ(returned.available_connections, 3u);
    EXPECT_EQ(returned.active_connections, 0u);
}

TEST_F(RedisHiredisTest, PoolAllowsConcurrentBorrowers) {
    auto& manager = im::db::redis_manager();

    auto worker = [&manager](int index) {
        return manager.execute([index](auto& redis) {
            redis.set(key("pool-" + std::to_string(index)), std::to_string(index));
            return redis.get(key("pool-" + std::to_string(index))).value_or("");
        });
    };

    auto f1 = std::async(std::launch::async, worker, 1);
    auto f2 = std::async(std::launch::async, worker, 2);
    auto f3 = std::async(std::launch::async, worker, 3);

    EXPECT_EQ(f1.get(), "1");
    EXPECT_EQ(f2.get(), "2");
    EXPECT_EQ(f3.get(), "3");

    auto stats = manager.get_pool_stats();
    EXPECT_EQ(stats.total_connections, 3u);
    EXPECT_EQ(stats.available_connections, 3u);
    EXPECT_EQ(stats.active_connections, 0u);
}

TEST_F(RedisHiredisTest, ReinitializeDoesNotReturnOldBorrowedConnectionToNewPool) {
    auto& manager = im::db::redis_manager();

    auto old_conn = manager.get_connection();
    ASSERT_TRUE(old_conn.is_valid());
    auto borrowed = manager.get_pool_stats();
    EXPECT_EQ(borrowed.available_connections, 2u);
    EXPECT_EQ(borrowed.active_connections, 1u);

    im::db::RedisConfig smaller = test_config();
    smaller.pool_size = 1;
    ASSERT_TRUE(manager.initialize(smaller));

    auto reinitialized = manager.get_pool_stats();
    EXPECT_EQ(reinitialized.total_connections, 1u);
    EXPECT_EQ(reinitialized.available_connections, 1u);
    EXPECT_EQ(reinitialized.active_connections, 0u);

    old_conn = {};

    auto after_old_return = manager.get_pool_stats();
    EXPECT_EQ(after_old_return.total_connections, 1u);
    EXPECT_EQ(after_old_return.available_connections, 1u);
    EXPECT_EQ(after_old_return.active_connections, 0u);
}

TEST_F(RedisHiredisTest, PoolExhaustionUsesConfiguredWaitTimeout) {
    auto& manager = im::db::redis_manager();

    im::db::RedisConfig config = test_config();
    config.pool_size = 1;
    config.pool_wait_timeout = 100;
    ASSERT_TRUE(manager.initialize(config));

    auto held = manager.get_connection();
    ASSERT_TRUE(held.is_valid());

    const auto start = std::chrono::steady_clock::now();
    EXPECT_THROW(manager.get_connection(), std::runtime_error);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1000);

    held = {};
    auto stats = manager.get_pool_stats();
    EXPECT_EQ(stats.total_connections, 1u);
    EXPECT_EQ(stats.available_connections, 1u);
    EXPECT_EQ(stats.active_connections, 0u);
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
