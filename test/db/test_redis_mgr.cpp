#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <future>
#include <fstream>
#include "../../common/database/redis_mgr.hpp"

namespace im::db::test {

class RedisManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试配置文件
        CreateTestConfig();
        
        // 初始化日志（如果需要）
        try {
            im::utils::LogManager::SetLogToConsole("redis_manager");
            im::utils::LogManager::GetLogger("redis_manager")->set_level(spdlog::level::debug);
        } catch (...) {
            // 如果日志初始化失败，忽略
        }
    }

    void TearDown() override {
        // 清理Redis管理器
        auto& mgr = im::db::redis_manager();
        mgr.shutdown();
        
        // 清理测试数据
        CleanupTestData();
        
        // 删除测试配置文件
        std::remove("test_config.json");
    }

    void CreateTestConfig() {
        std::ofstream config_file("test_config.json");
        config_file << R"({
            "redis": {
                "host": "127.0.0.1",
                "port": 6379,
                "password": "",
                "db": 1,
                "pool_size": 5,
                "connect_timeout": 2000,
                "socket_timeout": 2000
            }
        })";
        config_file.close();
    }

    void CleanupTestData() {
        try {
            auto& mgr = im::db::redis_manager();
            if (mgr.is_healthy()) {
                mgr.execute([](sw::redis::Redis& redis) {
                    // 清理测试键
                    std::vector<std::string> test_patterns = {
                        "test:*", "user:test:*", "counter:test:*", 
                        "hash:test:*", "list:test:*", "set:test:*", "zset:test:*"
                    };
                    
                    for (const auto& pattern : test_patterns) {
                        std::vector<std::string> keys;
                        redis.keys(pattern, std::back_inserter(keys));
                        if (!keys.empty()) {
                            redis.del(keys.begin(), keys.end());
                        }
                    }
                    return true;
                });
            }
        } catch (...) {
            // 忽略清理错误
        }
    }

    bool IsRedisAvailable() {
        try {
            im::db::RedisConfig config;
            config.host = "127.0.0.1";
            config.port = 6379;
            config.password = "myself";  // 添加密码
            config.db = 1;
            config.pool_size = 1;
            
            auto redis = std::make_shared<sw::redis::Redis>(config.to_connection_options());
            redis->ping();
            return true;
        } catch (...) {
            return false;
        }
    }
};

// =========================== RedisConfig 测试 ===========================

TEST_F(RedisManagerTest, RedisConfig_DefaultValues) {
    im::db::RedisConfig config;
    
    EXPECT_EQ(config.host, "127.0.0.1");
    EXPECT_EQ(config.port, 6379);
    EXPECT_EQ(config.password, "");
    EXPECT_EQ(config.db, 0);
    EXPECT_EQ(config.pool_size, 10);
    EXPECT_EQ(config.connect_timeout, 1000);
    EXPECT_EQ(config.socket_timeout, 1000);
}

TEST_F(RedisManagerTest, RedisConfig_FromFile) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto config = im::db::RedisConfig::from_file("test_config.json");
    
    EXPECT_EQ(config.host, "127.0.0.1");
    EXPECT_EQ(config.port, 6379);
    EXPECT_EQ(config.db, 1);
    EXPECT_EQ(config.pool_size, 5);
    EXPECT_EQ(config.connect_timeout, 2000);
    EXPECT_EQ(config.socket_timeout, 2000);
}

TEST_F(RedisManagerTest, RedisConfig_FromFile_InvalidFile) {
    // ConfigManager 在文件不存在时不抛出异常，而是使用默认值
    // 这里测试文件不存在时返回默认配置
    auto config = im::db::RedisConfig::from_file("nonexistent_config.json");
    
    // 应该返回默认配置值
    EXPECT_EQ(config.host, "127.0.0.1");
    EXPECT_EQ(config.port, 6379);
    EXPECT_EQ(config.db, 0);  // 默认值
    EXPECT_EQ(config.pool_size, 10);  // 默认值
}

TEST_F(RedisManagerTest, RedisConfig_ToConnectionOptions) {
    im::db::RedisConfig config;
    config.host = "localhost";
    config.port = 6380;
    config.password = "secret";
    config.db = 2;
    config.connect_timeout = 5000;
    config.socket_timeout = 3000;
    
    auto opts = config.to_connection_options();
    
    EXPECT_EQ(opts.host, "localhost");
    EXPECT_EQ(opts.port, 6380);
    EXPECT_EQ(opts.password, "secret");
    EXPECT_EQ(opts.db, 2);
    EXPECT_EQ(opts.connect_timeout, std::chrono::milliseconds(5000));
    EXPECT_EQ(opts.socket_timeout, std::chrono::milliseconds(3000));
}

// =========================== RedisManager 初始化测试 ===========================

TEST_F(RedisManagerTest, Initialize_FromConfigFile) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    EXPECT_TRUE(mgr.initialize("test_config.json"));
    EXPECT_TRUE(mgr.is_healthy());
}

TEST_F(RedisManagerTest, Initialize_FromConfigObject) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    im::db::RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.db = 1;
    config.pool_size = 3;
    
    auto& mgr = im::db::redis_manager();
    EXPECT_TRUE(mgr.initialize(config));
    EXPECT_TRUE(mgr.is_healthy());
}

TEST_F(RedisManagerTest, Initialize_InvalidConfig) {
    im::db::RedisConfig config;
    config.host = "invalid_host_12345";
    config.port = 12345;
    config.connect_timeout = 100;  // 设置很短的连接超时时间
    config.socket_timeout = 100;   // 设置很短的 socket 超时时间
    
    auto& mgr = im::db::redis_manager();
    EXPECT_FALSE(mgr.initialize(config));
}

TEST_F(RedisManagerTest, Initialize_AlreadyInitialized) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    EXPECT_TRUE(mgr.initialize("test_config.json"));
    EXPECT_TRUE(mgr.initialize("test_config.json")); // 第二次初始化应该返回true但不重新初始化
}

// =========================== 连接管理测试 ===========================

TEST_F(RedisManagerTest, GetConnection_ValidConnection) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    auto conn = mgr.get_connection();
    EXPECT_TRUE(conn.is_valid());
    
    // 测试连接可用性
    EXPECT_NO_THROW(conn->ping());
}

TEST_F(RedisManagerTest, GetConnection_NotInitialized) {
    auto& mgr = im::db::redis_manager();
    EXPECT_THROW(mgr.get_connection(), std::runtime_error);
}

TEST_F(RedisManagerTest, RedisConnection_RAII) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    auto initial_stats = mgr.get_pool_stats();
    
    {
        auto conn = mgr.get_connection();
        EXPECT_TRUE(conn.is_valid());
        
        auto current_stats = mgr.get_pool_stats();
        EXPECT_EQ(current_stats.active_connections, initial_stats.active_connections + 1);
        EXPECT_EQ(current_stats.available_connections, initial_stats.available_connections - 1);
    } // conn应该在这里析构并归还连接
    
    auto final_stats = mgr.get_pool_stats();
    EXPECT_EQ(final_stats.active_connections, initial_stats.active_connections);
    EXPECT_EQ(final_stats.available_connections, initial_stats.available_connections);
}

// =========================== 基础Redis操作测试 ===========================

TEST_F(RedisManagerTest, Execute_StringOperations) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    // 测试set和get
    mgr.execute([](sw::redis::Redis& redis) {
        redis.set("test:string:key1", "value1");
        return true;
    });
    
    auto result = mgr.execute([](sw::redis::Redis& redis) {
        return redis.get("test:string:key1");
    });
    
    ASSERT_TRUE(bool(result));
    EXPECT_EQ(*result, "value1");
    
    // 测试exists和del
    auto exists = mgr.execute([](sw::redis::Redis& redis) {
        return redis.exists("test:string:key1");
    });
    EXPECT_GT(exists, 0);
    
    auto deleted = mgr.execute([](sw::redis::Redis& redis) {
        return redis.del("test:string:key1");
    });
    EXPECT_GT(deleted, 0);
    
    auto not_exists = mgr.execute([](sw::redis::Redis& redis) {
        return redis.exists("test:string:key1");
    });
    EXPECT_EQ(not_exists, 0);
}

TEST_F(RedisManagerTest, Execute_HashOperations) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    // 测试hset和hget
    mgr.execute([](sw::redis::Redis& redis) {
        redis.hset("test:hash:user1", "name", "John");
        redis.hset("test:hash:user1", "email", "john@example.com");
        redis.hset("test:hash:user1", "age", "30");
        return true;
    });
    
    auto name = mgr.execute([](sw::redis::Redis& redis) {
        return redis.hget("test:hash:user1", "name");
    });
    ASSERT_TRUE(bool(name));
    EXPECT_EQ(*name, "John");
    
    // 测试hgetall
    auto profile = mgr.execute([](sw::redis::Redis& redis) {
        std::unordered_map<std::string, std::string> result;
        redis.hgetall("test:hash:user1", std::inserter(result, result.begin()));
        return result;
    });
    
    EXPECT_EQ(profile.size(), 3);
    EXPECT_EQ(profile["name"], "John");
    EXPECT_EQ(profile["email"], "john@example.com");
    EXPECT_EQ(profile["age"], "30");
    
    // 测试hexists和hdel
    auto field_exists = mgr.execute([](sw::redis::Redis& redis) {
        return redis.hexists("test:hash:user1", "name");
    });
    EXPECT_TRUE(field_exists);
    
    mgr.execute([](sw::redis::Redis& redis) {
        redis.hdel("test:hash:user1", "age");
        return true;
    });
    
    auto age_exists = mgr.execute([](sw::redis::Redis& redis) {
        return redis.hexists("test:hash:user1", "age");
    });
    EXPECT_FALSE(age_exists);
}

TEST_F(RedisManagerTest, Execute_ListOperations) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    // 测试lpush和rpush
    mgr.execute([](sw::redis::Redis& redis) {
        redis.lpush("test:list:queue", "item1");
        redis.lpush("test:list:queue", "item2");
        redis.rpush("test:list:queue", "item3");
        return true;
    });
    
    // 测试llen
    auto length = mgr.execute([](sw::redis::Redis& redis) {
        return redis.llen("test:list:queue");
    });
    EXPECT_EQ(length, 3);
    
    // 测试lrange
    auto items = mgr.execute([](sw::redis::Redis& redis) {
        std::vector<std::string> result;
        redis.lrange("test:list:queue", 0, -1, std::back_inserter(result));
        return result;
    });
    
    EXPECT_EQ(items.size(), 3);
    EXPECT_EQ(items[0], "item2"); // 最后lpush的在前面
    EXPECT_EQ(items[1], "item1");
    EXPECT_EQ(items[2], "item3"); // rpush的在后面
    
    // 测试lpop和rpop
    auto left_item = mgr.execute([](sw::redis::Redis& redis) {
        return redis.lpop("test:list:queue");
    });
    ASSERT_TRUE(bool(left_item));
    EXPECT_EQ(*left_item, "item2");
    
    auto right_item = mgr.execute([](sw::redis::Redis& redis) {
        return redis.rpop("test:list:queue");
    });
    ASSERT_TRUE(bool(right_item));
    EXPECT_EQ(*right_item, "item3");
}

TEST_F(RedisManagerTest, Execute_SetOperations) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    // 测试sadd
    mgr.execute([](sw::redis::Redis& redis) {
        redis.sadd("test:set:tags", "cpp");
        redis.sadd("test:set:tags", "redis");
        redis.sadd("test:set:tags", "database");
        redis.sadd("test:set:tags", "cpp"); // 重复添加
        return true;
    });
    
    // 测试scard
    auto size = mgr.execute([](sw::redis::Redis& redis) {
        return redis.scard("test:set:tags");
    });
    EXPECT_EQ(size, 3); // 去重后应该是3个
    
    // 测试sismember
    auto is_member = mgr.execute([](sw::redis::Redis& redis) {
        return redis.sismember("test:set:tags", "cpp");
    });
    EXPECT_TRUE(is_member);
    
    auto not_member = mgr.execute([](sw::redis::Redis& redis) {
        return redis.sismember("test:set:tags", "java");
    });
    EXPECT_FALSE(not_member);
    
    // 测试smembers
    auto members = mgr.execute([](sw::redis::Redis& redis) {
        std::unordered_set<std::string> result;
        redis.smembers("test:set:tags", std::inserter(result, result.begin()));
        return result;
    });
    
    EXPECT_EQ(members.size(), 3);
    EXPECT_TRUE(members.count("cpp"));
    EXPECT_TRUE(members.count("redis"));
    EXPECT_TRUE(members.count("database"));
    
    // 测试srem
    mgr.execute([](sw::redis::Redis& redis) {
        redis.srem("test:set:tags", "database");
        return true;
    });
    
    auto new_size = mgr.execute([](sw::redis::Redis& redis) {
        return redis.scard("test:set:tags");
    });
    EXPECT_EQ(new_size, 2);
}

TEST_F(RedisManagerTest, Execute_SortedSetOperations) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    // 测试zadd
    mgr.execute([](sw::redis::Redis& redis) {
        redis.zadd("test:zset:leaderboard", "player1", 100.0);
        redis.zadd("test:zset:leaderboard", "player2", 200.0);
        redis.zadd("test:zset:leaderboard", "player3", 150.0);
        return true;
    });
    
    // 测试zcard
    auto size = mgr.execute([](sw::redis::Redis& redis) {
        return redis.zcard("test:zset:leaderboard");
    });
    EXPECT_EQ(size, 3);
    
    // 测试zscore
    auto score = mgr.execute([](sw::redis::Redis& redis) {
        return redis.zscore("test:zset:leaderboard", "player2");
    });
    ASSERT_TRUE(bool(score));
    EXPECT_DOUBLE_EQ(*score, 200.0);
    
    // 测试zrange (按分数排序)
    auto players = mgr.execute([](sw::redis::Redis& redis) {
        std::vector<std::string> result;
        redis.zrange("test:zset:leaderboard", 0, -1, std::back_inserter(result));
        return result;
    });
    
    EXPECT_EQ(players.size(), 3);
    EXPECT_EQ(players[0], "player1"); // 分数最低
    EXPECT_EQ(players[1], "player3");
    EXPECT_EQ(players[2], "player2"); // 分数最高
    
    // 测试zrem
    mgr.execute([](sw::redis::Redis& redis) {
        redis.zrem("test:zset:leaderboard", "player1");
        return true;
    });
    
    auto new_size = mgr.execute([](sw::redis::Redis& redis) {
        return redis.zcard("test:zset:leaderboard");
    });
    EXPECT_EQ(new_size, 2);
}

// =========================== 高级操作测试 ===========================

TEST_F(RedisManagerTest, Execute_PipelineOperations) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    auto replies = mgr.execute([](sw::redis::Redis& redis) {
        auto pipe = redis.pipeline();
        
        // 批量设置
        pipe.set("test:pipeline:key1", "value1");
        pipe.set("test:pipeline:key2", "value2");
        pipe.set("test:pipeline:key3", "value3");
        
        // 批量获取
        pipe.get("test:pipeline:key1");
        pipe.get("test:pipeline:key2");
        pipe.get("test:pipeline:key3");
        
        return pipe.exec();
    });
    
    EXPECT_EQ(replies.size(), 6); // 3个set + 3个get
    
    // 检查GET操作的结果（使用QueuedReplies的get接口）
    for (size_t i = 3; i < 6; ++i) {
        std::string expected_value = "value" + std::to_string(i - 2);
        auto val = replies.get<std::string>(i);
        EXPECT_EQ(val, expected_value);
    }
}

TEST_F(RedisManagerTest, Execute_TransactionOperations) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    auto replies = mgr.execute([](sw::redis::Redis& redis) {
        auto tx = redis.transaction();
        
        // 原子操作
        tx.set("test:tx:counter", "0");
        tx.incr("test:tx:counter");
        tx.incr("test:tx:counter");
        tx.incr("test:tx:counter");
        tx.get("test:tx:counter");
        
        return tx.exec();
    });
    
    EXPECT_EQ(replies.size(), 5); // set + 3个incr + get
    
    // 检查最后的GET结果（index 4）
    auto last_val = replies.get<std::string>(4);
    EXPECT_EQ(last_val, "3");
}

// =========================== SafeExecute测试 ===========================

TEST_F(RedisManagerTest, SafeExecute_SuccessCase) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    mgr.execute([](sw::redis::Redis& redis) {
        redis.set("test:safe:existing_key", "100");
        return true;
    });
    
    auto result = mgr.safe_execute([](sw::redis::Redis& redis) {
        auto val = redis.get("test:safe:existing_key");
        return val ? std::stoi(*val) : 0;
    }, -1);
    
    EXPECT_EQ(result, 100);
}

TEST_F(RedisManagerTest, SafeExecute_DefaultValue) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    auto result = mgr.safe_execute([](sw::redis::Redis& redis) {
        auto val = redis.get("test:safe:nonexistent_key");
        return val ? std::stoi(*val) : 0;
    }, -1);
    
    EXPECT_EQ(result, 0); // lambda内部处理了不存在的情况
}

TEST_F(RedisManagerTest, SafeExecute_ExceptionHandling) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    auto result = mgr.safe_execute([](sw::redis::Redis& redis) -> int {
        // 故意抛出异常
        throw std::runtime_error("Test exception");
        return 42;
    }, -999);
    
    EXPECT_EQ(result, -999); // 应该返回默认值
}

// =========================== 并发测试 ===========================

TEST_F(RedisManagerTest, ConcurrentAccess) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    const int thread_count = 10;
    const int operations_per_thread = 50;
    std::vector<std::future<int>> futures;
    
    // 启动多个线程并发访问
    for (int i = 0; i < thread_count; ++i) {
        futures.push_back(std::async(std::launch::async, [&mgr, i, operations_per_thread]() {
            int success_count = 0;
            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    mgr.execute([i, j](sw::redis::Redis& redis) {
                        std::string key = "test:concurrent:thread_" + std::to_string(i) + "_op_" + std::to_string(j);
                        std::string value = "value_" + std::to_string(i) + "_" + std::to_string(j);
                        redis.set(key, value);
                        
                        auto result = redis.get(key);
                        return result && *result == value;
                    });
                    success_count++;
                } catch (...) {
                    // 记录失败但继续
                }
            }
            return success_count;
        }));
    }
    
    // 等待所有线程完成
    int total_success = 0;
    for (auto& future : futures) {
        total_success += future.get();
    }
    
    // 验证大部分操作成功
    int expected_total = thread_count * operations_per_thread;
    EXPECT_GE(total_success, expected_total * 0.95); // 至少95%成功
    
    // 验证连接池状态正常
    auto stats = mgr.get_pool_stats();
    EXPECT_GT(stats.total_connections, 0);
    EXPECT_EQ(stats.active_connections, 0); // 所有连接应该已归还
}

TEST_F(RedisManagerTest, ConnectionPoolExhaustion) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    // 使用很小的连接池进行测试
    im::db::RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.db = 1;
    config.pool_size = 2; // 只有2个连接
    
    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize(config));
    
    std::vector<std::future<bool>> futures;
    const int thread_count = 5; // 超过连接池大小
    
    for (int i = 0; i < thread_count; ++i) {
        futures.push_back(std::async(std::launch::async, [&mgr, i]() {
            try {
                return mgr.execute([i](sw::redis::Redis& redis) {
                    // 模拟较长的操作
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    redis.set("test:exhaustion:" + std::to_string(i), "value");
                    return true;
                });
            } catch (...) {
                return false;
            }
        }));
    }
    
    // 等待所有操作完成
    int success_count = 0;
    for (auto& future : futures) {
        if (future.get()) {
            success_count++;
        }
    }
    
    // 所有操作都应该成功（可能需要等待）
    EXPECT_EQ(success_count, thread_count);
}

// =========================== 监控和健康检查测试 ===========================

TEST_F(RedisManagerTest, PoolStats) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    auto stats = mgr.get_pool_stats();
    EXPECT_EQ(stats.total_connections, 5); // 配置文件中设置的pool_size
    EXPECT_EQ(stats.available_connections, 5);
    EXPECT_EQ(stats.active_connections, 0);
    
    // 获取一个连接
    {
        auto conn = mgr.get_connection();
        auto stats_with_connection = mgr.get_pool_stats();
        EXPECT_EQ(stats_with_connection.available_connections, 4);
        EXPECT_EQ(stats_with_connection.active_connections, 1);
    }
    
    // 连接归还后统计应该恢复
    auto final_stats = mgr.get_pool_stats();
    EXPECT_EQ(final_stats.available_connections, 5);
    EXPECT_EQ(final_stats.active_connections, 0);
}

TEST_F(RedisManagerTest, HealthCheck) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    
    // 未初始化时应该不健康
    EXPECT_FALSE(mgr.is_healthy());
    
    // 初始化后应该健康
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    EXPECT_TRUE(mgr.is_healthy());
    
    // 关闭后应该不健康
    mgr.shutdown();
    EXPECT_FALSE(mgr.is_healthy());
}

TEST_F(RedisManagerTest, Shutdown) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    EXPECT_TRUE(mgr.is_healthy());
    
    // 关闭管理器
    mgr.shutdown();
    
    // 关闭后操作应该失败
    EXPECT_THROW(mgr.get_connection(), std::runtime_error);
    EXPECT_FALSE(mgr.is_healthy());
    
    auto stats = mgr.get_pool_stats();
    EXPECT_EQ(stats.total_connections, 0);
    EXPECT_EQ(stats.available_connections, 0);
    EXPECT_EQ(stats.active_connections, 0);
}

// =========================== 错误处理测试 ===========================

TEST_F(RedisManagerTest, Execute_NotInitialized) {
    auto& mgr = im::db::redis_manager();
    
    EXPECT_THROW(mgr.execute([](sw::redis::Redis& redis) {
        return redis.ping();
    }), std::runtime_error);
}

TEST_F(RedisManagerTest, Execute_InvalidOperation) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    // 测试无效的Redis操作
    EXPECT_THROW(mgr.execute([](sw::redis::Redis& redis) {
        // 尝试对非列表键执行列表操作
        redis.set("test:error:not_a_list", "string_value");
        return redis.lpop("test:error:not_a_list");
    }), std::exception);
}

// =========================== 压力测试 ===========================

TEST_F(RedisManagerTest, StressTest) {
    if (!IsRedisAvailable()) {
        GTEST_SKIP() << "Redis server not available";
    }

    auto& mgr = im::db::redis_manager();
    ASSERT_TRUE(mgr.initialize("test_config.json"));
    
    const int total_operations = 1000;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    
    std::vector<std::thread> threads;
    const int thread_count = 20;
    const int ops_per_thread = total_operations / thread_count;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&mgr, &success_count, &error_count, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                try {
                    mgr.execute([t, i](sw::redis::Redis& redis) {
                        // 混合操作
                        std::string key = "stress:test:" + std::to_string(t) + ":" + std::to_string(i);
                        
                        // 字符串操作
                        redis.set(key + ":str", "value_" + std::to_string(i));
                        redis.get(key + ":str");
                        
                        // 哈希操作
                        redis.hset(key + ":hash", "field1", "value1");
                        redis.hget(key + ":hash", "field1");
                        
                        // 计数器操作
                        redis.incr(key + ":counter");
                        
                        return true;
                    });
                    success_count++;
                } catch (...) {
                    error_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 验证结果
    EXPECT_EQ(success_count + error_count, total_operations);
    EXPECT_GE(success_count, total_operations * 0.95); // 至少95%成功率
    EXPECT_LT(duration.count(), 30000); // 应该在30秒内完成
    
    // 检查连接池状态
    auto final_stats = mgr.get_pool_stats();
    EXPECT_EQ(final_stats.active_connections, 0); // 所有连接都应该归还
    
    std::cout << "压力测试结果:" << std::endl;
    std::cout << "总操作数: " << total_operations << std::endl;
    std::cout << "成功数: " << success_count << std::endl;
    std::cout << "错误数: " << error_count << std::endl;
    std::cout << "耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "QPS: " << (total_operations * 1000.0 / duration.count()) << std::endl;
}

} // namespace im::db::test

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}