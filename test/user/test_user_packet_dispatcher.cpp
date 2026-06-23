#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <nlohmann/json.hpp>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <command.pb.h>
#include <password_hasher.hpp>
#include <user.hpp>
#include <user_packet_dispatcher.hpp>
#include <user_service.hpp>

#include "../support/postgres_schema.hpp"

namespace {

using json = nlohmann::json;

const char* kConnStr =
        "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1720000000000;

class UserPacketDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        Cleanup();
        service_ = std::make_unique<im::service::user::UserService>(
                db_, std::make_unique<im::service::user::PasswordHasher>());
        dispatcher_ = std::make_unique<im::service::user::UserPacketDispatcher>(
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
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'packet-user-test-%')");
        t.commit();
    }

    std::string CreateUser(const std::string& account,
                           const std::string& nickname = "") {
        im::service::user::RegisterRequest request;
        request.account = account;
        request.password = "securePass123";
        request.nickname = nickname.empty() ? account + "-nickname" : nickname;
        request.now_ms = kNowMs;
        auto result = service_->register_user(request);
        EXPECT_TRUE(result.ok) << result.message;
        return result.profile.uid;
    }

    im::user::UserPacketRequest JsonPacket(uint32_t cmd_id,
                                             const json& payload,
                                             const std::string& actor_uid = "") {
        im::user::UserPacketRequest packet;
        packet.mutable_header()->set_cmd_id(cmd_id);
        packet.mutable_header()->set_from_uid(actor_uid);
        packet.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs));
        packet.mutable_header()->set_device_id("header-device");
        packet.mutable_header()->set_platform("desktop");
        packet.set_type_name("application/json");
        packet.set_payload(payload.dump());
        return packet;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<im::service::user::UserService> service_;
    std::unique_ptr<im::service::user::UserPacketDispatcher> dispatcher_;
};

}  // namespace

TEST_F(UserPacketDispatcherTest, RegisterJsonPacketCreatesProfileAndAuthHint) {
    json body;
    body["account"] = "packet-user-test-register";
    body["password"] = "securePass123";
    body["nickname"] = "Packet Register";
    body["device_id"] = "body-device";
    body["platform"] = "web";

    const auto result = dispatcher_->handle(
            JsonPacket(im::command::CMD_REGISTER, body));

    ASSERT_EQ(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 201);
    EXPECT_EQ(result.type_name(), "application/json");
    EXPECT_TRUE(result.auth_token_hint().required());
    EXPECT_FALSE(result.auth_token_hint().uid().empty());
    EXPECT_EQ(result.auth_token_hint().account(), "packet-user-test-register");
    EXPECT_EQ(result.auth_token_hint().device_id(), "body-device");
    EXPECT_EQ(result.auth_token_hint().platform(), "web");

    const auto response = json::parse(result.payload());
    EXPECT_EQ(response["profile"]["account"], "packet-user-test-register");
    EXPECT_EQ(response["profile"]["nickname"], "Packet Register");
}

TEST_F(UserPacketDispatcherTest, LoginJsonPacketReturnsProfileAndHeaderAuthHint) {
    const std::string uid = CreateUser("packet-user-test-login", "Packet Login");

    json body;
    body["account"] = "packet-user-test-login";
    body["password"] = "securePass123";
    const auto result = dispatcher_->handle(
            JsonPacket(im::command::CMD_LOGIN, body));

    ASSERT_EQ(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 200);
    EXPECT_TRUE(result.auth_token_hint().required());
    EXPECT_EQ(result.auth_token_hint().uid(), uid);
    EXPECT_EQ(result.auth_token_hint().account(), "packet-user-test-login");
    EXPECT_EQ(result.auth_token_hint().device_id(), "header-device");
    EXPECT_EQ(result.auth_token_hint().platform(), "desktop");

    const auto response = json::parse(result.payload());
    EXPECT_EQ(response["profile"]["uid"], uid);
    EXPECT_EQ(response["profile"]["account"], "packet-user-test-login");
}

TEST_F(UserPacketDispatcherTest, ProfileUpdateAndSearchAreHandledInsideServiceBoundary) {
    const std::string uid = CreateUser("packet-user-test-profile", "Original Nick");
    CreateUser("packet-user-test-search", "Searchable Nick");

    const auto profile_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_GET_USER_INFO, json::object(), uid));
    ASSERT_EQ(profile_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(profile_result.http_status(), 200);
    EXPECT_EQ(json::parse(profile_result.payload())["profile"]["nickname"],
              "Original Nick");

    json update_body;
    update_body["nickname"] = "Updated Nick";
    update_body["avatar"] = "avatar://updated";
    update_body["gender"] = 2;
    update_body["signature"] = "updated signature";
    const auto update_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_UPDATE_USER_INFO, update_body, uid));

    ASSERT_EQ(update_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(update_result.http_status(), 200);
    const auto update_response = json::parse(update_result.payload());
    EXPECT_EQ(update_response["profile"]["nickname"], "Updated Nick");
    EXPECT_EQ(update_response["profile"]["avatar"], "avatar://updated");
    EXPECT_EQ(update_response["profile"]["gender"], 2);
    EXPECT_EQ(update_response["profile"]["signature"], "updated signature");

    json search_body;
    search_body["q"] = "packet-user-test-search";
    const auto search_result = dispatcher_->handle(
            JsonPacket(im::command::CMD_SEARCH_USER, search_body, uid));

    ASSERT_EQ(search_result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(search_result.http_status(), 200);
    const auto search_response = json::parse(search_result.payload());
    ASSERT_GE(search_response["users"].size(), 1u);
    EXPECT_EQ(search_response["users"][0]["account"], "packet-user-test-search");
}

TEST_F(UserPacketDispatcherTest, InvalidJsonPacketReturnsBadRequest) {
    auto packet = JsonPacket(im::command::CMD_LOGIN, json::object());
    packet.set_payload("{bad-json");

    const auto result = dispatcher_->handle(packet);

    EXPECT_NE(result.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(result.http_status(), 400);
    EXPECT_EQ(result.base().error_code(), im::base::INVALID_REQUEST);
    EXPECT_EQ(json::parse(result.payload())["err_msg"], "Invalid JSON body");
}
