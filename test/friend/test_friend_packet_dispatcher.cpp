#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <command.pb.h>
#include <friend_packet_dispatcher.hpp>
#include <friend_service.hpp>
#include <password_hasher.hpp>
#include <user_service.hpp>

#include "../support/postgres_schema.hpp"

namespace {

using json = nlohmann::json;

const char* kConnStr =
        "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1719000000000;

class FriendPacketDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTables();
        Cleanup();
        user_service_ = std::make_shared<im::service::user::UserService>(
                db_, std::make_unique<im::service::user::PasswordHasher>());
        friend_service_ = std::make_unique<im::service::friend_::FriendService>(
                db_, user_service_);
        dispatcher_ = std::make_unique<im::service::friend_::FriendPacketDispatcher>(
                friend_service_.get());
    }

    void TearDown() override {
        dispatcher_.reset();
        friend_service_.reset();
        user_service_.reset();
        Cleanup();
    }

    static void EnsureTables() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void Cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_friends"
            WHERE "requester_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'packet-friend-test-%'
                )
               OR "target_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'packet-friend-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'packet-friend-test-%')");
        t.commit();
    }

    std::string CreateUser(const std::string& account) {
        im::service::user::RegisterRequest request;
        request.account = account;
        request.password = "securePass123";
        request.nickname = account + "-nickname";
        request.now_ms = kNowMs;
        auto result = user_service_->register_user(request);
        EXPECT_TRUE(result.ok) << result.message;
        return result.profile.uid;
    }

    im::friend_::FriendPacketRequest JsonPacket(uint32_t cmd_id,
                                                  const std::string& actor_uid,
                                                  const json& payload) {
        im::friend_::FriendPacketRequest packet;
        packet.mutable_header()->set_cmd_id(cmd_id);
        packet.mutable_header()->set_from_uid(actor_uid);
        packet.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs));
        packet.set_type_name("application/json");
        packet.set_payload(payload.dump());
        return packet;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::unique_ptr<im::service::friend_::FriendService> friend_service_;
    std::unique_ptr<im::service::friend_::FriendPacketDispatcher> dispatcher_;
};

}  // namespace

TEST_F(FriendPacketDispatcherTest, AddRespondAndListJsonPackets) {
    const std::string uid_a = CreateUser("packet-friend-test-flow-a");
    const std::string uid_b = CreateUser("packet-friend-test-flow-b");

    json add_body;
    add_body["target_uid"] = uid_b;
    auto add_packet = JsonPacket(im::command::CMD_ADD_FRIEND, uid_a, add_body);
    const auto add_result = dispatcher_->handle(add_packet);

    ASSERT_EQ(add_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(add_result.http_status(), 201);
    EXPECT_EQ(add_result.type_name(), "application/json");
    EXPECT_EQ(json::parse(add_result.payload()).at("message"), "Friend request sent");

    auto pending_packet = JsonPacket(
            im::command::CMD_GET_FRIEND_REQUESTS, uid_b, json::object());
    const auto pending_result = dispatcher_->handle(pending_packet);

    ASSERT_EQ(pending_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(pending_result.http_status(), 200);
    const auto pending_json = json::parse(pending_result.payload());
    ASSERT_EQ(pending_json.at("pending_requests").size(), 1u);
    EXPECT_EQ(pending_json["pending_requests"][0]["friend_uid"], uid_a);
    const auto friend_id = pending_json["pending_requests"][0]["friend_id"].get<uint64_t>();

    json accept_body;
    accept_body["friend_id"] = friend_id;
    accept_body["accept"] = true;
    auto accept_packet = JsonPacket(
            im::command::CMD_HANDLE_FRIEND_REQUEST, uid_b, accept_body);
    const auto accept_result = dispatcher_->handle(accept_packet);

    ASSERT_EQ(accept_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(accept_result.http_status(), 200);
    EXPECT_EQ(json::parse(accept_result.payload()).at("message"),
              "Friend request accepted");

    auto list_packet = JsonPacket(im::command::CMD_GET_FRIEND_LIST, uid_a, json::object());
    const auto list_result = dispatcher_->handle(list_packet);

    ASSERT_EQ(list_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(list_result.http_status(), 200);
    const auto list_json = json::parse(list_result.payload());
    ASSERT_EQ(list_json.at("friends").size(), 1u);
    EXPECT_EQ(list_json["friends"][0]["friend_uid"], uid_b);
}

TEST_F(FriendPacketDispatcherTest, AddJsonPacketRejectsMissingTarget) {
    const std::string uid_a = CreateUser("packet-friend-test-missing-a");

    auto packet = JsonPacket(im::command::CMD_ADD_FRIEND, uid_a, json::object());
    const auto result = dispatcher_->handle(packet);

    EXPECT_NE(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 400);
    EXPECT_EQ(result.base().error_code(), im::base::PARAM_ERROR);
}
