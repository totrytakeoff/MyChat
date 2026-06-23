#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <command.pb.h>
#include <group_message_service.hpp>
#include <group_packet_dispatcher.hpp>
#include <group_service.hpp>
#include <password_hasher.hpp>
#include <user_service.hpp>

#include "../support/postgres_schema.hpp"

namespace {

using json = nlohmann::json;

const char* kConnStr =
        "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1721000000000;

class GroupPacketDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTables();
        Cleanup();
        user_service_ = std::make_shared<im::service::user::UserService>(
                db_, std::make_unique<im::service::user::PasswordHasher>());
        group_service_ = std::make_shared<im::service::group::GroupService>(
                db_, user_service_);
        group_message_service_ =
                std::make_unique<im::service::group::GroupMessageService>(
                        db_, user_service_, group_service_);
        dispatcher_ = std::make_unique<im::service::group::GroupPacketDispatcher>(
                group_service_.get(), group_message_service_.get());
    }

    void TearDown() override {
        dispatcher_.reset();
        group_message_service_.reset();
        group_service_.reset();
        user_service_.reset();
        Cleanup();
    }

    static void EnsureTables() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void Cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_group_messages"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'packet-group-test-%'
                )
               OR "sender_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'packet-group-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_group_members"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'packet-group-test-%'
                )
               OR "user_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'packet-group-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_groups"
            WHERE "creator_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'packet-group-test-%'
                )
               OR "name" LIKE 'packet-group-test-%')");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'packet-group-test-%')");
        t.commit();
    }

    std::string CreateUser(const std::string& account) {
        im::service::user::RegisterRequest request;
        request.account = account;
        request.password = "securePass123";
        request.nickname = account + "-nickname";
        request.now_ms = kNowMs;
        const auto result = user_service_->register_user(request);
        EXPECT_TRUE(result.ok) << result.message;
        return result.profile.uid;
    }

    im::group::GroupPacketRequest JsonPacket(uint32_t cmd_id,
                                               const std::string& actor_uid,
                                               const json& payload,
                                               int64_t timestamp = kNowMs) {
        im::group::GroupPacketRequest packet;
        packet.mutable_header()->set_cmd_id(cmd_id);
        packet.mutable_header()->set_from_uid(actor_uid);
        packet.mutable_header()->set_timestamp(static_cast<uint64_t>(timestamp));
        packet.set_type_name("application/json");
        packet.set_payload(payload.dump());
        return packet;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::shared_ptr<im::service::group::GroupService> group_service_;
    std::unique_ptr<im::service::group::GroupMessageService> group_message_service_;
    std::unique_ptr<im::service::group::GroupPacketDispatcher> dispatcher_;
};

}  // namespace

TEST_F(GroupPacketDispatcherTest, CreateJoinListMembersSendAndHistoryPackets) {
    const std::string owner = CreateUser("packet-group-test-flow-owner");
    const std::string member = CreateUser("packet-group-test-flow-member");

    json create_body;
    create_body["name"] = "packet-group-test-flow-room";
    const auto create_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_CREATE_GROUP, owner, create_body));

    ASSERT_EQ(create_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(create_result.http_status(), 201);
    const auto create_json = json::parse(create_result.payload());
    const auto group_id = create_json.at("group_id").get<uint64_t>();
    ASSERT_GT(group_id, 0u);

    json join_body;
    join_body["group_id"] = group_id;
    const auto join_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_APPLY_JOIN_GROUP, member, join_body, kNowMs + 1));
    ASSERT_EQ(join_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(join_result.http_status(), 200);

    const auto list_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_GET_GROUP_LIST, owner, json::object()));
    ASSERT_EQ(list_result.base().error_code(), im::base::SUCCESS);
    const auto list_json = json::parse(list_result.payload());
    ASSERT_EQ(list_json.at("groups").size(), 1u);
    EXPECT_EQ(list_json["groups"][0]["group_id"].get<uint64_t>(), group_id);

    json members_body;
    members_body["group_id"] = group_id;
    const auto members_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_GET_GROUP_MEMBERS, owner, members_body));
    ASSERT_EQ(members_result.base().error_code(), im::base::SUCCESS);
    const auto members_json = json::parse(members_result.payload());
    ASSERT_EQ(members_json.at("members").size(), 2u);

    json send_body;
    send_body["group_id"] = group_id;
    send_body["content"] = "hello group packet";
    const auto send_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_SEND_GROUP_MESSAGE, member, send_body, kNowMs + 2));
    ASSERT_EQ(send_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(send_result.http_status(), 201);
    const auto send_json = json::parse(send_result.payload());
    ASSERT_GT(send_json.at("msg_id").get<uint64_t>(), 0u);
    ASSERT_EQ(send_result.push_events_size(), 1);
    EXPECT_TRUE(send_result.push_events(0).enabled());
    EXPECT_EQ(send_result.push_events(0).receiver_uid(), owner);
    EXPECT_EQ(send_result.push_events(0).sender_uid(), member);
    EXPECT_EQ(send_result.push_events(0).conversation_type(), "group");
    EXPECT_EQ(send_result.push_events(0).conversation_id(), std::to_string(group_id));

    json history_body;
    history_body["group_id"] = group_id;
    history_body["before"] = kNowMs + 1000;
    history_body["limit"] = 10;
    const auto history_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_GET_GROUP_MESSAGES, owner, history_body));
    ASSERT_EQ(history_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(history_result.http_status(), 200);
    const auto history_json = json::parse(history_result.payload());
    ASSERT_EQ(history_json.at("messages").size(), 1u);
    EXPECT_EQ(history_json["messages"][0]["content"], "hello group packet");
}

TEST_F(GroupPacketDispatcherTest, NonMemberCannotSendOrReadHistory) {
    const std::string owner = CreateUser("packet-group-test-forbid-owner");
    const std::string stranger = CreateUser("packet-group-test-forbid-stranger");

    json create_body;
    create_body["name"] = "packet-group-test-forbid-room";
    const auto create_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_CREATE_GROUP, owner, create_body));
    ASSERT_EQ(create_result.base().error_code(), im::base::SUCCESS);
    const auto group_id = json::parse(create_result.payload())
                                  .at("group_id")
                                  .get<uint64_t>();

    json send_body;
    send_body["group_id"] = group_id;
    send_body["content"] = "forbidden";
    const auto send_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_SEND_GROUP_MESSAGE, stranger, send_body));
    EXPECT_NE(send_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(send_result.base().error_code(), im::base::PERMISSION_DENIED);
    EXPECT_EQ(send_result.http_status(), 403);

    json history_body;
    history_body["group_id"] = group_id;
    const auto history_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_GET_GROUP_MESSAGES, stranger, history_body));
    EXPECT_NE(history_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(history_result.base().error_code(), im::base::PERMISSION_DENIED);
    EXPECT_EQ(history_result.http_status(), 403);
}

TEST_F(GroupPacketDispatcherTest, InvalidJsonPacketReturnsBadRequest) {
    const std::string owner = CreateUser("packet-group-test-invalid-owner");
    auto packet = JsonPacket(im::command::CMD_CREATE_GROUP, owner, json::object());
    packet.set_payload("{bad-json");

    const auto result = dispatcher_->handle(packet);

    EXPECT_NE(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.base().error_code(), im::base::INVALID_REQUEST);
    EXPECT_EQ(result.http_status(), 400);
    EXPECT_EQ(json::parse(result.payload())["err_msg"], "Invalid JSON body");
}
