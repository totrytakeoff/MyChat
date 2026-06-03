#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <message.hpp>
#include <message-odb.hxx>

#include <database/redis/redis_mgr.hpp>

#include <gateway/push_service.hpp>
#include <gateway/fanout_policies.hpp>
#include <gateway/connection_manager/connection_manager.hpp>
#include <message_service.hpp>
#include <utils/log_manager.hpp>

namespace {

using im::gateway::AllSessionsFanoutPolicy;
using im::gateway::ConnectionManager;
using im::gateway::DeviceSessionInfo;
using im::gateway::FanoutPolicy;
using im::gateway::NewestSessionFanoutPolicy;
using im::gateway::PlatformFilterFanoutPolicy;
using im::gateway::PushService;
using im::db::RedisConfig;
using im::db::redis_manager;
using im::service::message::MessageService;
using im::utils::LogManager;

RedisConfig test_redis_config() {
    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "mychat-dev-pass";
    config.db = 15;
    config.connect_timeout = 1000;
    config.socket_timeout = 1000;
    return config;
}

std::string config_path() {
    return (std::filesystem::path(MYCHAT_SOURCE_DIR) / "config/dev.json").string();
}

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

DeviceSessionInfo make_session(const std::string& sid,
                                const std::string& device,
                                const std::string& platform,
                                std::chrono::system_clock::time_point connect_time) {
    DeviceSessionInfo info;
    info.session_id = sid;
    info.device_id = device;
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

        msg_service_ = std::make_shared<MessageService>(db_);
    }

    void TearDown() override {
        CleanupTestData();
        push_service_.reset();
        conn_mgr_.reset();
        msg_service_.reset();
        redis_manager().shutdown();
    }

    static void EnsureTable() {
        odb::pgsql::database db(kConnStr);
        odb::transaction t(db.begin());
        db.execute(R"(
            CREATE TABLE IF NOT EXISTS "im_messages" (
                "msg_id" BIGSERIAL NOT NULL PRIMARY KEY,
                "sender_uid" TEXT NOT NULL,
                "receiver_uid" TEXT NOT NULL,
                "content" TEXT NOT NULL,
                "msg_type" INTEGER NOT NULL,
                "status" INTEGER NOT NULL,
                "create_time" BIGINT NOT NULL,
                "delivered_time" BIGINT NOT NULL,
                "read_time" BIGINT NOT NULL
            )
        )");
        t.commit();
    }

    void CleanupTestData() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_messages"
            WHERE "sender_uid" LIKE 'task8-test-%'
               OR "receiver_uid" LIKE 'task8-test-%')");
        t.commit();
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<MessageService> msg_service_;
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::unique_ptr<PushService> push_service_;
};

// --- PushService integration tests ---

TEST_F(PushServiceTest, NullDepsReturnsSilently) {
    push_service_ = std::make_unique<PushService>(nullptr, nullptr, nullptr);
    push_service_->push_to_user("task8-test-user", 1, "hello");

    push_service_.reset();
    push_service_ = std::make_unique<PushService>(nullptr, nullptr, msg_service_);
    push_service_->push_to_user("task8-test-user", 1, "hello");
}

TEST_F(PushServiceTest, NoActiveSessions) {
    conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);
    push_service_ = std::make_unique<PushService>(conn_mgr_.get(), nullptr, msg_service_);

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

// --- FanoutPolicy unit tests ---

TEST_F(PushServiceTest, AllSessionsFanoutSelectsAll) {
    AllSessionsFanoutPolicy policy;
    auto now = std::chrono::system_clock::now();
    std::vector<DeviceSessionInfo> sessions = {
        make_session("sess-1", "dev-1", "web", now),
        make_session("sess-2", "dev-2", "mobile", now),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 2u);
    EXPECT_EQ(selected[0], "sess-1");
    EXPECT_EQ(selected[1], "sess-2");
}

TEST_F(PushServiceTest, PlatformFilterSingleMatch) {
    PlatformFilterFanoutPolicy policy({"web"});
    auto now = std::chrono::system_clock::now();
    std::vector<DeviceSessionInfo> sessions = {
        make_session("web-sess", "dev-1", "web", now),
        make_session("mobile-sess", "dev-2", "mobile", now),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], "web-sess");
}

TEST_F(PushServiceTest, PlatformFilterMultiplePlatforms) {
    PlatformFilterFanoutPolicy policy({"web", "desktop"});
    auto now = std::chrono::system_clock::now();
    std::vector<DeviceSessionInfo> sessions = {
        make_session("web-sess", "dev-1", "web", now),
        make_session("mobile-sess", "dev-2", "mobile", now),
        make_session("desktop-sess", "dev-3", "desktop", now),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 2u);
    EXPECT_EQ(selected[0], "web-sess");
    EXPECT_EQ(selected[1], "desktop-sess");
}

TEST_F(PushServiceTest, PlatformFilterNoMatch) {
    PlatformFilterFanoutPolicy policy({"tablet"});
    auto now = std::chrono::system_clock::now();
    std::vector<DeviceSessionInfo> sessions = {
        make_session("web-sess", "dev-1", "web", now),
        make_session("mobile-sess", "dev-2", "mobile", now),
    };

    auto selected = policy.select_sessions(sessions);
    EXPECT_TRUE(selected.empty());
}

TEST_F(PushServiceTest, PlatformFilterEmptyAllowedList) {
    PlatformFilterFanoutPolicy policy(std::vector<std::string>{});
    auto now = std::chrono::system_clock::now();
    std::vector<DeviceSessionInfo> sessions = {
        make_session("web-sess", "dev-1", "web", now),
    };

    auto selected = policy.select_sessions(sessions);
    EXPECT_TRUE(selected.empty());
}

TEST_F(PushServiceTest, NewestSessionSelectsLatest) {
    NewestSessionFanoutPolicy policy;
    auto t1 = std::chrono::system_clock::now();
    auto t2 = t1 + std::chrono::seconds(10);
    auto t3 = t1 + std::chrono::seconds(20);
    std::vector<DeviceSessionInfo> sessions = {
        make_session("old-sess", "dev-1", "web", t1),
        make_session("mid-sess", "dev-2", "mobile", t2),
        make_session("new-sess", "dev-3", "desktop", t3),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], "new-sess");
}

TEST_F(PushServiceTest, NewestSessionSingleSession) {
    NewestSessionFanoutPolicy policy;
    auto now = std::chrono::system_clock::now();
    std::vector<DeviceSessionInfo> sessions = {
        make_session("only-sess", "dev-1", "web", now),
    };

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], "only-sess");
}

TEST_F(PushServiceTest, NewestSessionNoSessions) {
    NewestSessionFanoutPolicy policy;
    std::vector<DeviceSessionInfo> sessions;

    auto selected = policy.select_sessions(sessions);
    EXPECT_TRUE(selected.empty());
}

TEST_F(PushServiceTest, CustomFanoutPolicyInjection) {
    conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);
    push_service_ = std::make_unique<PushService>(conn_mgr_.get(), nullptr, msg_service_);

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
