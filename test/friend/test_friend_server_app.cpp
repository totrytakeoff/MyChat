#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <friend_server_app.hpp>
#include <password_hasher.hpp>
#include <user_service.hpp>

#include "base.pb.h"
#include "friend.grpc.pb.h"

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1718500000000;

void ensure_schema() {
    odb::pgsql::database db(kConnStr);
    im::test::EnsureCoreSchema(db);
}

void cleanup_data() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_friends"
        WHERE "requester_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-friend-server-test-%'
            )
           OR "target_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-friend-server-test-%'
            ))");
    db.execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'grpc-friend-server-test-%')");
    t.commit();
}

std::string create_user(const std::string& account) {
    auto db = std::make_shared<odb::pgsql::database>(kConnStr);
    auto user_service = std::make_shared<im::service::user::UserService>(
        db, std::make_unique<im::service::user::PasswordHasher>());

    im::service::user::RegisterRequest request;
    request.account = account;
    request.password = "securePass123";
    request.nickname = account + "-nickname";
    request.now_ms = kNowMs;
    auto result = user_service->register_user(request);
    EXPECT_TRUE(result.ok) << result.message;
    return result.profile.uid;
}

std::unique_ptr<im::friend_::FriendService::Stub> make_stub(int port) {
    const std::string endpoint = "127.0.0.1:" + std::to_string(port);
    auto channel = ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials());
    return im::friend_::FriendService::NewStub(channel);
}

}  // namespace

TEST(FriendServerAppTest, SendRespondAndListThroughGrpc) {
    ensure_schema();
    cleanup_data();

    const std::string uid_a = create_user("grpc-friend-server-test-flow-a");
    const std::string uid_b = create_user("grpc-friend-server-test-flow-b");

    im::service::friend_::FriendServerConfig config;
    config.listen_address = "127.0.0.1:0";
    config.postgres_connection_string = kConnStr;

    im::service::friend_::FriendServerApp server(config);
    ASSERT_TRUE(server.start());
    ASSERT_GT(server.selected_port(), 0);

    auto stub = make_stub(server.selected_port());

    im::friend_::AddFriendRequest send_request;
    send_request.mutable_header()->set_from_uid(uid_a);
    send_request.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs));
    send_request.set_to_uid(uid_b);
    im::friend_::AddFriendResponse send_response;
    ::grpc::ClientContext send_context;
    auto status = stub->SendRequest(&send_context, send_request, &send_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(send_response.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(send_response.has_request());
    ASSERT_FALSE(send_response.request().request_id().empty());

    im::friend_::HandleFriendRequest accept_request;
    accept_request.mutable_header()->set_from_uid(uid_b);
    accept_request.set_request_id(send_response.request().request_id());
    accept_request.set_accept(true);
    im::friend_::HandleFriendResponse accept_response;
    ::grpc::ClientContext accept_context;
    status = stub->RespondToRequest(&accept_context, accept_request, &accept_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(accept_response.base().error_code(), im::base::SUCCESS);

    im::friend_::GetFriendListRequest list_request;
    list_request.mutable_header()->set_from_uid(uid_a);
    im::friend_::GetFriendListResponse list_response;
    ::grpc::ClientContext list_context;
    status = stub->GetFriends(&list_context, list_request, &list_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(list_response.base().error_code(), im::base::SUCCESS);
    ASSERT_EQ(list_response.friends_size(), 1);
    EXPECT_EQ(list_response.friends(0).uid(), uid_b);
    EXPECT_EQ(list_response.friends(0).status(), im::friend_::ACCEPTED);

    server.shutdown();
    cleanup_data();
}

TEST(FriendServerAppTest, StartReturnsFalseForInvalidListenAddress) {
    im::service::friend_::FriendServerConfig config;
    config.listen_address = "127.0.0.1:not-a-port";
    config.postgres_connection_string = kConnStr;

    im::service::friend_::FriendServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}

TEST(FriendServerAppTest, StartReturnsFalseWithoutPostgresConnectionString) {
    im::service::friend_::FriendServerConfig config;
    config.listen_address = "127.0.0.1:0";

    im::service::friend_::FriendServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}
