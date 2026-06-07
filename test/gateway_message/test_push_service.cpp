#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <message.hpp>
#include <message-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/http/message_client.hpp>
#include <gateway/push/push_service.hpp>
#include <gateway/connection_manager/connection_manager.hpp>
#include <fanout_policy.hpp>
#include <message_service.hpp>
#include <utils/log_manager.hpp>

#include "../support/postgres_schema.hpp"

namespace {

using im::gateway::ConnectionManager;
using im::gateway::LocalMessageClient;
using im::gateway::PushService;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::service::message::MessageService;
using im::service::push::AllSessionsFanoutPolicy;
using im::service::push::NewestSessionFanoutPolicy;
using im::service::push::PlatformFilterFanoutPolicy;
using im::service::push::PushSessionInfo;
using im::utils::LogManager;

RedisConfig test_redis_config() {
    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "mychat-dev-pass";
    config.db = 15;
    config.pool_size = 4;
    config.connect_timeout = 1000;
    config.socket_timeout = 1000;
    return config;
}

std::string config_path() {
    return (std::filesystem::path(MYCHAT_SOURCE_DIR) / "config/dev.json").string();
}

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

PushSessionInfo make_session(const std::string& sid,
                              const std::string& platform,
                              std::chrono::system_clock::time_point connect_time) {
    PushSessionInfo info;
    info.session_id = sid;
    info.platform = platform;
    info.connect_time = connect_time;
    return info;
}

class PushServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("push_service");
        LogManager::SetLogToConsole("redis_manager");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        CleanupTestData();
        CleanupRedisTestData();

        msg_service_ = std::make_shared<MessageService>(db_);
        msg_client_ = std::make_shared<LocalMessageClient>(msg_service_);
    }

    void TearDown() override {
        CleanupRedisTestData();
        CleanupTestData();
        push_service_.reset();
        conn_mgr_.reset();
        msg_client_.reset();
        msg_service_.reset();
        redis_manager().shutdown();
    }

    static void EnsureTable() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void CleanupTestData() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_messages"
            WHERE "sender_uid" LIKE 'task8-test-%'
               OR "receiver_uid" LIKE 'task8-test-%')");
        t.commit();
    }

    static void CleanupRedisTestData() {
        redis_manager().safe_execute(
            [](auto& redis) {
                for (int i = 0; i < 8; ++i) {
                    const std::string user = "task8-test-pool-user-" + std::to_string(i);
                    redis.del("user:sessions:" + user);
                    redis.del("user:platform:" + user);
                    redis.srem("online:users", user);
                    for (int j = 0; j < 3; ++j) {
                        redis.del("session:user:task8-test-pool-session-" +
                                  std::to_string(i) + "-" + std::to_string(j));
                    }
                }
                return true;
            },
            false);
    }

    static void SeedRedisSessions(const std::string& user, int user_index) {
        redis_manager().execute([&](auto& redis) {
            const auto now = std::chrono::system_clock::now();
            const auto sessions_key = "user:sessions:" + user;
            const auto devices_key = "user:platform:" + user;
            for (int j = 0; j < 3; ++j) {
                im::gateway::DeviceSessionInfo info;
                info.session_id = "task8-test-pool-session-" +
                                  std::to_string(user_index) + "-" + std::to_string(j);
                info.device_id = "task8-test-pool-device-" + std::to_string(j);
                info.platform = (j % 2 == 0) ? "web" : "mobile";
                info.connect_time = now + std::chrono::seconds(j);

                const auto field = info.device_id + ":" + info.platform;
                redis.hset(sessions_key, field, info.to_json().dump());
                redis.sadd(devices_key, field);
                redis.hset("session:user:" + info.session_id, "user_id", user);
                redis.hset("session:user:" + info.session_id, "device_id", info.device_id);
                redis.hset("session:user:" + info.session_id, "platform", info.platform);
            }
            redis.sadd("online:users", user);
            return true;
        });
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MessageService> msg_service_;
    std::shared_ptr<LocalMessageClient> msg_client_;
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::unique_ptr<PushService> push_service_;
};

// --- PushService integration tests ---

TEST_F(PushServiceTest, NullDepsReturnsSilently) {
    push_service_ = std::make_unique<PushService>(nullptr, nullptr, nullptr);
    push_service_->push_to_user("task8-test-user", 1, "hello");

    push_service_.reset();
    push_service_ = std::make_unique<PushService>(nullptr, nullptr, msg_client_);
    push_service_->push_to_user("task8-test-user", 1, "hello");
}

TEST_F(PushServiceTest, NoActiveSessions) {
    conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);
    push_service_ = std::make_unique<PushService>(conn_mgr_.get(), nullptr, msg_client_);

    auto result = msg_service_->send_text_message({
        .sender_uid = "task8-test-push-sender",
        .receiver_uid = "task8-test-push-rec",
        .content = "test push",
        .msg_type = im::service::message::MessageType::TEXT,
        .now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
    });
    ASSERT_TRUE(result.ok);

    push_service_->push_to_user("task8-test-push-rec", result.data.msg_id, "test push");

    auto msgs = msg_service_->pull_offline("task8-test-push-rec", INT64_MAX, 10);
    bool found = false;
    for (const auto& m : msgs) {
        if (m.msg_id == result.data.msg_id) {
            EXPECT_EQ(m.delivered_time, 0) << "Message should stay undelivered with no sessions";
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(PushServiceTest, ConcurrentConnectionManagerSessionReadsUseRedisPool) {
    conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);

    constexpr int kUsers = 8;
    for (int i = 0; i < kUsers; ++i) {
        SeedRedisSessions("task8-test-pool-user-" + std::to_string(i), i);
    }

    std::vector<std::future<bool>> futures;
    futures.reserve(kUsers);
    for (int i = 0; i < kUsers; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i] {
            const std::string user = "task8-test-pool-user-" + std::to_string(i);
            auto sessions = conn_mgr_->get_user_sessions(user);
            if (sessions.size() != 3u) {
                return false;
            }
            if (!conn_mgr_->is_user_online_on_platform(user, "web")) {
                return false;
            }
            if (conn_mgr_->get_online_count() < 1u) {
                return false;
            }
            return true;
        }));
    }

    for (auto& future : futures) {
        EXPECT_TRUE(future.get());
    }

    auto stats = redis_manager().get_pool_stats();
    EXPECT_EQ(stats.total_connections, 4u);
    EXPECT_EQ(stats.available_connections, 4u);
    EXPECT_EQ(stats.active_connections, 0u);
}

TEST_F(PushServiceTest, ConcurrentPushSessionLookupUsesRedisPoolWithoutLiveSessions) {
    conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);
    push_service_ = std::make_unique<PushService>(conn_mgr_.get(), nullptr, msg_client_);

    constexpr int kUsers = 8;
    std::vector<uint64_t> msg_ids;
    msg_ids.reserve(kUsers);
    for (int i = 0; i < kUsers; ++i) {
        const std::string receiver = "task8-test-pool-user-" + std::to_string(i);
        SeedRedisSessions(receiver, i);
        auto result = msg_service_->send_text_message({
            .sender_uid = "task8-test-pool-sender",
            .receiver_uid = receiver,
            .content = "pool push lookup",
            .msg_type = im::service::message::MessageType::TEXT,
            .now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
        });
        ASSERT_TRUE(result.ok);
        msg_ids.push_back(result.data.msg_id);
    }

    std::vector<std::future<void>> futures;
    futures.reserve(kUsers);
    for (int i = 0; i < kUsers; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i, &msg_ids] {
            const std::string receiver = "task8-test-pool-user-" + std::to_string(i);
            push_service_->push_to_user(receiver, msg_ids[i], "pool push lookup");
        }));
    }

    for (auto& future : futures) {
        future.get();
    }

    for (int i = 0; i < kUsers; ++i) {
        auto msgs = msg_service_->pull_offline("task8-test-pool-user-" + std::to_string(i),
                                               INT64_MAX, 10);
        bool found = false;
        for (const auto& msg : msgs) {
            if (msg.msg_id == msg_ids[i]) {
                EXPECT_EQ(msg.delivered_time, 0)
                    << "No live WebSocketServer means push stays best-effort undelivered";
                found = true;
            }
        }
        EXPECT_TRUE(found);
    }

    auto stats = redis_manager().get_pool_stats();
    EXPECT_EQ(stats.total_connections, 4u);
    EXPECT_EQ(stats.available_connections, 4u);
    EXPECT_EQ(stats.active_connections, 0u);
}

// --- FanoutPolicy unit tests ---

TEST_F(PushServiceTest, AllSessionsFanoutSelectsAll) {
    AllSessionsFanoutPolicy policy;
    auto now = std::chrono::system_clock::now();
    std::vector<PushSessionInfo> sessions = {
        make_session("sess-1", "web", now),
        make_session("sess-2", "mobile", now),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 2u);
    EXPECT_EQ(selected[0], "sess-1");
    EXPECT_EQ(selected[1], "sess-2");
}

TEST_F(PushServiceTest, PlatformFilterSingleMatch) {
    PlatformFilterFanoutPolicy policy({"web"});
    auto now = std::chrono::system_clock::now();
    std::vector<PushSessionInfo> sessions = {
        make_session("web-sess", "web", now),
        make_session("mobile-sess", "mobile", now),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], "web-sess");
}

TEST_F(PushServiceTest, PlatformFilterMultiplePlatforms) {
    PlatformFilterFanoutPolicy policy({"web", "desktop"});
    auto now = std::chrono::system_clock::now();
    std::vector<PushSessionInfo> sessions = {
        make_session("web-sess", "web", now),
        make_session("mobile-sess", "mobile", now),
        make_session("desktop-sess", "desktop", now),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 2u);
    EXPECT_EQ(selected[0], "web-sess");
    EXPECT_EQ(selected[1], "desktop-sess");
}

TEST_F(PushServiceTest, PlatformFilterNoMatch) {
    PlatformFilterFanoutPolicy policy({"tablet"});
    auto now = std::chrono::system_clock::now();
    std::vector<PushSessionInfo> sessions = {
        make_session("web-sess", "web", now),
        make_session("mobile-sess", "mobile", now),
    };

    auto selected = policy.select_sessions(sessions);
    EXPECT_TRUE(selected.empty());
}

TEST_F(PushServiceTest, PlatformFilterEmptyAllowedList) {
    PlatformFilterFanoutPolicy policy(std::vector<std::string>{});
    auto now = std::chrono::system_clock::now();
    std::vector<PushSessionInfo> sessions = {
        make_session("web-sess", "web", now),
    };

    auto selected = policy.select_sessions(sessions);
    EXPECT_TRUE(selected.empty());
}

TEST_F(PushServiceTest, NewestSessionSelectsLatest) {
    NewestSessionFanoutPolicy policy;
    auto t1 = std::chrono::system_clock::now();
    auto t2 = t1 + std::chrono::seconds(10);
    auto t3 = t1 + std::chrono::seconds(20);
    std::vector<PushSessionInfo> sessions = {
        make_session("old-sess", "web", t1),
        make_session("mid-sess", "mobile", t2),
        make_session("new-sess", "desktop", t3),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], "new-sess");
}

TEST_F(PushServiceTest, NewestSessionSingleSession) {
    NewestSessionFanoutPolicy policy;
    auto now = std::chrono::system_clock::now();
    std::vector<PushSessionInfo> sessions = {
        make_session("only-sess", "web", now),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], "only-sess");
}

TEST_F(PushServiceTest, NewestSessionNoSessions) {
    NewestSessionFanoutPolicy policy;
    std::vector<PushSessionInfo> sessions;

    auto selected = policy.select_sessions(sessions);
    EXPECT_TRUE(selected.empty());
}

TEST_F(PushServiceTest, CustomFanoutPolicyInjection) {
    conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);
    push_service_ = std::make_unique<PushService>(conn_mgr_.get(), nullptr, msg_client_);

    auto custom_policy = std::make_unique<PlatformFilterFanoutPolicy>(
        std::vector<std::string>{"web"});
    push_service_->set_fanout_policy(std::move(custom_policy));

    auto result = msg_service_->send_text_message({
        .sender_uid = "task8-test-custom-sender",
        .receiver_uid = "task8-test-custom-rec",
        .content = "custom policy test",
        .msg_type = im::service::message::MessageType::TEXT,
        .now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
    });
    ASSERT_TRUE(result.ok);

    push_service_->push_to_user("task8-test-custom-rec", result.data.msg_id, "custom policy test");
}

} // anonymous namespace
