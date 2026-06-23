#include <gtest/gtest.h>

#include <climits>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <command.pb.h>
#include <message.hpp>
#include <message_packet_dispatcher.hpp>
#include <message_service.hpp>

#include "../support/postgres_schema.hpp"

namespace {

using json = nlohmann::json;

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1718400000000;

class MessagePacketDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        Cleanup();
        service_ = std::make_unique<im::service::message::MessageService>(db_);
        dispatcher_ = std::make_unique<im::service::message::MessagePacketDispatcher>(
                service_.get());
    }

    void TearDown() override {
        dispatcher_.reset();
        service_.reset();
        Cleanup();
    }

    static void EnsureTable() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void Cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_messages"
            WHERE "sender_uid" LIKE 'packet-message-test-%'
               OR "receiver_uid" LIKE 'packet-message-test-%')");
        t.commit();
    }

    im::service::message::SendResult Send(const std::string& sender,
                                          const std::string& receiver,
                                          const std::string& content,
                                          int64_t timestamp) {
        im::service::message::SendRequest request;
        request.sender_uid = sender;
        request.receiver_uid = receiver;
        request.content = content;
        request.msg_type = im::service::message::MessageType::TEXT;
        request.now_ms = timestamp;
        return service_->send_text_message(request);
    }

    im::message::MessagePacketRequest JsonPacket(uint32_t cmd_id,
                                                   const std::string& actor_uid,
                                                   const json& payload) {
        im::message::MessagePacketRequest packet;
        packet.mutable_header()->set_cmd_id(cmd_id);
        packet.mutable_header()->set_from_uid(actor_uid);
        packet.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs));
        packet.set_type_name("application/json");
        packet.set_payload(payload.dump());
        return packet;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<im::service::message::MessageService> service_;
    std::unique_ptr<im::service::message::MessagePacketDispatcher> dispatcher_;
};

} // namespace

TEST_F(MessagePacketDispatcherTest, HistoryJsonPacketReturnsConversation) {
    ASSERT_TRUE(Send("packet-message-test-history-a",
                     "packet-message-test-history-b",
                     "first history",
                     kNowMs).ok);
    ASSERT_TRUE(Send("packet-message-test-history-b",
                     "packet-message-test-history-a",
                     "second history",
                     kNowMs + 1000).ok);

    json request_body;
    request_body["peer_uid"] = "packet-message-test-history-b";
    request_body["before_time"] = std::to_string(kNowMs + 5000);
    request_body["limit"] = "10";

    auto packet = JsonPacket(im::command::CMD_MESSAGE_HISTORY,
                             "packet-message-test-history-a",
                             request_body);
    const auto result = dispatcher_->handle(packet);

    ASSERT_EQ(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 200);
    EXPECT_EQ(result.type_name(), "application/json");

    const auto response = json::parse(result.payload());
    ASSERT_EQ(response.at("count").get<size_t>(), 2u);
    ASSERT_EQ(response.at("messages").size(), 2u);
    EXPECT_EQ(response["messages"][0]["content"], "first history");
    EXPECT_EQ(response["messages"][1]["content"], "second history");
}

TEST_F(MessagePacketDispatcherTest, PullOfflineJsonPacketMarksDelivered) {
    auto sent = Send("packet-message-test-offline-a",
                     "packet-message-test-offline-b",
                     "offline packet",
                     kNowMs);
    ASSERT_TRUE(sent.ok);

    json request_body;
    request_body["before_time"] = std::to_string(kNowMs + 5000);
    request_body["limit"] = "10";

    auto packet = JsonPacket(im::command::CMD_PULL_MESSAGE,
                             "packet-message-test-offline-b",
                             request_body);
    const auto result = dispatcher_->handle(packet);

    ASSERT_EQ(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 200);

    const auto response = json::parse(result.payload());
    ASSERT_EQ(response.at("count").get<size_t>(), 1u);
    ASSERT_EQ(response.at("messages").size(), 1u);
    EXPECT_EQ(response["messages"][0]["msg_id"].get<uint64_t>(), sent.data.msg_id);
    EXPECT_EQ(response["messages"][0]["status"].get<int>(),
              static_cast<int>(im::service::message::MessageStatus::DELIVERED));
    EXPECT_GT(response["messages"][0]["delivered_time"].get<int64_t>(), 0);

    const auto remaining = service_->pull_offline(
            "packet-message-test-offline-b", INT64_MAX, 10);
    EXPECT_TRUE(remaining.empty());
}

TEST_F(MessagePacketDispatcherTest, HistoryJsonPacketRejectsMissingPeer) {
    auto packet = JsonPacket(im::command::CMD_MESSAGE_HISTORY,
                             "packet-message-test-history-a",
                             json::object());
    const auto result = dispatcher_->handle(packet);

    EXPECT_NE(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 400);
    EXPECT_EQ(result.base().error_code(), im::base::PARAM_ERROR);
}
