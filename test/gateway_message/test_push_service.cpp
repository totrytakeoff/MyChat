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
#include <gateway/connection_manager/connection_manager.hpp>
#include <message_service.hpp>
#include <utils/log_manager.hpp>

namespace {

using im::gateway::AllSessionsFanoutPolicy;
using im::gateway::ConnectionManager;
using im::gateway::DeviceSessionInfo;
using im::gateway::FanoutPolicy;
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

TEST_F(PushServiceTest, NullDepsReturnsSilently) {
    // All deps are nullptr.
    push_service_ = std::make_unique<PushService>(nullptr, nullptr, nullptr);

    // Should not crash or throw.
    push_service_->push_to_user("task8-test-user", 1, "hello");

    // Also test with msg_service present but conn_mgr/ws_server null.
    push_service_.reset();
    push_service_ = std::make_unique<PushService>(nullptr, nullptr, msg_service_);
    push_service_->push_to_user("task8-test-user", 1, "hello");
}

TEST_F(PushServiceTest, NoActiveSessions) {
    // ConnectionManager with no sessions, WebSocketServer is nullptr.
    conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);
    push_service_ = std::make_unique<PushService>(conn_mgr_.get(), nullptr, msg_service_);

    // Should not crash; message stays undelivered.
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

    // Verify message is NOT delivered (no sessions to push to).
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

TEST_F(PushServiceTest, AllSessionsFanoutSelectsAll) {
    AllSessionsFanoutPolicy policy;
    std::vector<DeviceSessionInfo> sessions;
    DeviceSessionInfo s1;
    s1.session_id = "sess-1";
    s1.device_id = "dev-1";
    s1.platform = "web";
    sessions.push_back(s1);

    DeviceSessionInfo s2;
    s2.session_id = "sess-2";
    s2.device_id = "dev-2";
    s2.platform = "mobile";
    sessions.push_back(s2);

    auto selected = policy.select_sessions(sessions);
    ASSERT_EQ(selected.size(), 2u);
    EXPECT_EQ(selected[0], "sess-1");
    EXPECT_EQ(selected[1], "sess-2");
}

// Custom fanout policy that only selects sessions matching a given platform.
class PlatformFilterFanoutPolicy : public FanoutPolicy {
public:
    explicit PlatformFilterFanoutPolicy(const std::string& platform)
        : target_platform_(platform) {}

    std::vector<std::string> select_sessions(
        const std::vector<DeviceSessionInfo>& sessions) override
    {
        std::vector<std::string> ids;
        for (const auto& s : sessions) {
            if (s.platform == target_platform_) {
                ids.push_back(s.session_id);
            }
        }
        return ids;
    }

private:
    std::string target_platform_;
};

TEST_F(PushServiceTest, CustomFanoutPolicyInjection) {
    // Create a PushService with a custom fanout policy.
    conn_mgr_ = std::make_unique<ConnectionManager>(config_path(), nullptr);
    push_service_ = std::make_unique<PushService>(conn_mgr_.get(), nullptr, msg_service_);

    auto custom_policy = std::make_unique<PlatformFilterFanoutPolicy>("web");
    FanoutPolicy* raw_policy = custom_policy.get();
    push_service_->set_fanout_policy(std::move(custom_policy));

    // Verify the policy was set (the PushService won't crash with it).
    // Can't actually push without live sessions, so just verify no crash.
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

    // Verify independent test of the custom policy itself.
    std::vector<DeviceSessionInfo> sessions;
    DeviceSessionInfo web_sess;
    web_sess.session_id = "web-sess";
    web_sess.platform = "web";
    sessions.push_back(web_sess);

    DeviceSessionInfo mobile_sess;
    mobile_sess.session_id = "mobile-sess";
    mobile_sess.platform = "mobile";
    sessions.push_back(mobile_sess);

    auto selected = raw_policy->select_sessions(sessions);
    ASSERT_EQ(selected.size(), 1u);
    EXPECT_EQ(selected[0], "web-sess");
}

} // anonymous namespace
