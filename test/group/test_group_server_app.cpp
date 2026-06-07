#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <group_server_app.hpp>
#include <password_hasher.hpp>
#include <user_service.hpp>

#include "base.pb.h"
#include "group.grpc.pb.h"

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1721100000000;

void ensure_schema() {
    odb::pgsql::database db(kConnStr);
    im::test::EnsureCoreSchema(db);
}

void cleanup_data() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_group_messages"
        WHERE "group_id" IN (
                SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'grpc-group-server-test-%'
            )
           OR "sender_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-group-server-test-%'
            ))");
    db.execute(R"(DELETE FROM "im_group_members"
        WHERE "group_id" IN (
                SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'grpc-group-server-test-%'
            )
           OR "user_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-group-server-test-%'
            ))");
    db.execute(R"(DELETE FROM "im_groups"
        WHERE "creator_uid" IN (
                SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-group-server-test-%'
            )
           OR "name" LIKE 'grpc-group-server-test-%')");
    db.execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'grpc-group-server-test-%')");
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

std::unique_ptr<im::group::GroupService::Stub> make_stub(int port) {
    const std::string endpoint = "127.0.0.1:" + std::to_string(port);
    auto channel = ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials());
    return im::group::GroupService::NewStub(channel);
}

}  // namespace

TEST(GroupServerAppTest, CreateJoinSendAndHistoryThroughGrpc) {
    ensure_schema();
    cleanup_data();

    const std::string owner = create_user("grpc-group-server-test-owner");
    const std::string member = create_user("grpc-group-server-test-member");

    im::service::group::GroupServerConfig config;
    config.listen_address = "127.0.0.1:0";
    config.postgres_connection_string = kConnStr;

    im::service::group::GroupServerApp server(config);
    ASSERT_TRUE(server.start());
    ASSERT_GT(server.selected_port(), 0);

    auto stub = make_stub(server.selected_port());

    im::group::CreateGroupRequest create_request;
    create_request.mutable_header()->set_from_uid(owner);
    create_request.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs));
    create_request.set_name("grpc-group-server-test-flow");
    im::group::CreateGroupResponse create_response;
    ::grpc::ClientContext create_context;
    auto status = stub->CreateGroup(&create_context, create_request, &create_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(create_response.base().error_code(), im::base::SUCCESS);
    ASSERT_GT(create_response.group_id(), 0);

    im::group::JoinGroupRequest join_request;
    join_request.mutable_header()->set_from_uid(member);
    join_request.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs + 1));
    join_request.set_group_id(create_response.group_id());
    im::group::GroupActionResponse join_response;
    ::grpc::ClientContext join_context;
    status = stub->JoinGroup(&join_context, join_request, &join_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(join_response.base().error_code(), im::base::SUCCESS);

    im::group::SendGroupMessageRequest send_request;
    send_request.mutable_header()->set_from_uid(member);
    send_request.set_group_id(create_response.group_id());
    send_request.set_content("server group grpc message");
    send_request.set_create_time(kNowMs + 2);
    im::group::SendGroupMessageResponse send_response;
    ::grpc::ClientContext send_context;
    status = stub->SendGroupMessage(&send_context, send_request, &send_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(send_response.base().error_code(), im::base::SUCCESS);
    ASSERT_GT(send_response.message().msg_id(), 0);

    im::group::GetGroupMessagesRequest history_request;
    history_request.mutable_header()->set_from_uid(owner);
    history_request.set_group_record_id(create_response.group_id());
    history_request.set_since(kNowMs + 1000);
    history_request.set_limit(10);
    im::group::GetGroupMessagesResponse history_response;
    ::grpc::ClientContext history_context;
    status = stub->GetGroupMessages(&history_context, history_request, &history_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(history_response.base().error_code(), im::base::SUCCESS);
    ASSERT_EQ(history_response.messages_size(), 1);
    EXPECT_EQ(history_response.messages(0).content(), "server group grpc message");

    server.shutdown();
    cleanup_data();
}

TEST(GroupServerAppTest, StartReturnsFalseForInvalidListenAddress) {
    im::service::group::GroupServerConfig config;
    config.listen_address = "127.0.0.1:not-a-port";
    config.postgres_connection_string = kConnStr;

    im::service::group::GroupServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}

TEST(GroupServerAppTest, StartReturnsFalseWithoutPostgresConnectionString) {
    im::service::group::GroupServerConfig config;
    config.listen_address = "127.0.0.1:0";

    im::service::group::GroupServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}
