#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <message.hpp>
#include <message-odb.hxx>

#include <message_service.hpp>

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1718000000000;
constexpr int64_t kLaterMs = 1718000005000;

class MessageServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        Cleanup();
    }

    void TearDown() override {
        Cleanup();
    }

    static void EnsureTable() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void Cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_messages"
            WHERE "sender_uid" LIKE 'task3-test-%'
               OR "receiver_uid" LIKE 'task3-test-%')");
        t.commit();
    }

    std::shared_ptr<odb::pgsql::database> db_;
};

} // anonymous namespace

TEST_F(MessageServiceTest, SendTextMessagePersists) {
    im::service::message::MessageService svc(db_);

    im::service::message::SendRequest req;
    req.sender_uid = "task3-test-sender-1";
    req.receiver_uid = "task3-test-receiver-1";
    req.content = "Hello, world!";
    req.msg_type = im::service::message::MessageType::TEXT;
    req.now_ms = kNowMs;

    auto result = svc.send_text_message(req);
    ASSERT_TRUE(result.ok);
    EXPECT_GT(result.data.msg_id, 0);
    EXPECT_EQ(result.data.sender_uid, "task3-test-sender-1");
    EXPECT_EQ(result.data.receiver_uid, "task3-test-receiver-1");
    EXPECT_EQ(result.data.content, "Hello, world!");
    EXPECT_EQ(result.data.create_time, kNowMs);
    EXPECT_EQ(result.data.status, im::service::message::MessageStatus::SENT);
}

TEST_F(MessageServiceTest, SendTextMessageEmptySenderRejected) {
    im::service::message::MessageService svc(db_);

    im::service::message::SendRequest req;
    req.sender_uid = "";
    req.receiver_uid = "task3-test-receiver-2";
    req.content = "Hello";
    req.now_ms = kNowMs;

    auto result = svc.send_text_message(req);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "EMPTY_SENDER");
}

TEST_F(MessageServiceTest, SendTextMessageEmptyReceiverRejected) {
    im::service::message::MessageService svc(db_);

    im::service::message::SendRequest req;
    req.sender_uid = "task3-test-sender-3";
    req.receiver_uid = "";
    req.content = "Hello";
    req.now_ms = kNowMs;

    auto result = svc.send_text_message(req);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "EMPTY_RECEIVER");
}

TEST_F(MessageServiceTest, SendTextMessageEmptyContentRejected) {
    im::service::message::MessageService svc(db_);

    im::service::message::SendRequest req;
    req.sender_uid = "task3-test-sender-4";
    req.receiver_uid = "task3-test-receiver-4";
    req.content = "";
    req.now_ms = kNowMs;

    auto result = svc.send_text_message(req);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "EMPTY_CONTENT");
}

TEST_F(MessageServiceTest, GetConversationReturnsMessagesInOrder) {
    im::service::message::MessageService svc(db_);

    // Send messages in non-chronological insert order (middle, last, first)
    // to verify ORDER BY create_time works regardless of insertion order.
    auto send = [&](int64_t t) -> im::service::message::SendResult {
        im::service::message::SendRequest req;
        req.sender_uid = "task3-test-cvs-a";
        req.receiver_uid = "task3-test-cvs-b";
        req.content = "msg at " + std::to_string(t);
        req.msg_type = im::service::message::MessageType::TEXT;
        req.now_ms = t;
        auto r = svc.send_text_message(req);
        EXPECT_TRUE(r.ok);
        return r;
    };

    send(kNowMs + 1000);
    send(kNowMs + 2000);
    send(kNowMs);

    // Get conversation — should be ordered by create_time ASC
    auto conv = svc.get_conversation("task3-test-cvs-a", "task3-test-cvs-b",
                                     INT64_MAX, 50);
    ASSERT_GE(conv.size(), 3);
    EXPECT_EQ(conv[0].content, "msg at " + std::to_string(kNowMs));
    EXPECT_EQ(conv[0].create_time, kNowMs);
    EXPECT_EQ(conv[1].content, "msg at " + std::to_string(kNowMs + 1000));
    EXPECT_EQ(conv[1].create_time, kNowMs + 1000);
    EXPECT_EQ(conv[2].content, "msg at " + std::to_string(kNowMs + 2000));
    EXPECT_EQ(conv[2].create_time, kNowMs + 2000);

    // Reverse direction should also work (user_b -> user_a)
    auto rev = svc.get_conversation("task3-test-cvs-b", "task3-test-cvs-a",
                                    INT64_MAX, 50);
    ASSERT_GE(rev.size(), 3);
    EXPECT_EQ(rev[0].create_time, kNowMs);
    EXPECT_EQ(rev[2].create_time, kNowMs + 2000);
}

TEST_F(MessageServiceTest, PullOfflineReturnsUndeliveredMessages) {
    im::service::message::MessageService svc(db_);

    // Send first message
    {
        im::service::message::SendRequest req;
        req.sender_uid = "task3-test-offline-a";
        req.receiver_uid = "task3-test-offline-b";
        req.content = "Offline test message 1";
        req.msg_type = im::service::message::MessageType::TEXT;
        req.now_ms = kNowMs;
        auto r = svc.send_text_message(req);
        ASSERT_TRUE(r.ok);
    }

    // Send second message at a later time
    {
        im::service::message::SendRequest req;
        req.sender_uid = "task3-test-offline-a";
        req.receiver_uid = "task3-test-offline-b";
        req.content = "Offline test message 2";
        req.msg_type = im::service::message::MessageType::TEXT;
        req.now_ms = kNowMs + 1000;
        auto r = svc.send_text_message(req);
        ASSERT_TRUE(r.ok);
    }

    // Pull offline for B — should be in chronological order
    auto offline = svc.pull_offline("task3-test-offline-b", INT64_MAX, 50);
    ASSERT_GE(offline.size(), 2);
    EXPECT_EQ(offline[0].sender_uid, "task3-test-offline-a");
    EXPECT_EQ(offline[0].content, "Offline test message 1");
    EXPECT_EQ(offline[0].create_time, kNowMs);
    EXPECT_EQ(offline[0].status, im::service::message::MessageStatus::SENT);
    EXPECT_EQ(offline[1].create_time, kNowMs + 1000);
}

TEST_F(MessageServiceTest, MarkDeliveredUpdatesStatus) {
    im::service::message::MessageService svc(db_);

    im::service::message::SendRequest req;
    req.sender_uid = "task3-test-del-a";
    req.receiver_uid = "task3-test-del-b";
    req.content = "Mark delivered test";
    req.msg_type = im::service::message::MessageType::TEXT;
    req.now_ms = kNowMs;

    auto send_result = svc.send_text_message(req);
    ASSERT_TRUE(send_result.ok);

    bool marked = svc.mark_delivered(send_result.data.msg_id, kLaterMs);
    EXPECT_TRUE(marked);

    // Verify offline pull no longer includes this message
    auto offline = svc.pull_offline("task3-test-del-b", INT64_MAX, 50);
    for (const auto& m : offline) {
        EXPECT_NE(m.msg_id, send_result.data.msg_id)
            << "Delivered message should not appear in offline pull";
    }
}

TEST_F(MessageServiceTest, MarkReadUpdatesStatus) {
    im::service::message::MessageService svc(db_);

    im::service::message::SendRequest req;
    req.sender_uid = "task3-test-read-a";
    req.receiver_uid = "task3-test-read-b";
    req.content = "Mark read test";
    req.msg_type = im::service::message::MessageType::TEXT;
    req.now_ms = kNowMs;

    auto send_result = svc.send_text_message(req);
    ASSERT_TRUE(send_result.ok);

    bool marked = svc.mark_delivered(send_result.data.msg_id, kLaterMs);
    EXPECT_TRUE(marked);

    marked = svc.mark_read(send_result.data.msg_id, kLaterMs + 1000);
    EXPECT_TRUE(marked);
}

TEST_F(MessageServiceTest, DeliveredAndReadMarkingRejectedForNonexistentMessage) {
    im::service::message::MessageService svc(db_);

    bool marked = svc.mark_delivered(99999999, kLaterMs);
    EXPECT_FALSE(marked);

    marked = svc.mark_read(99999999, kLaterMs);
    EXPECT_FALSE(marked);
}
