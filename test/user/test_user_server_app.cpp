#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <user_server_app.hpp>

#include "base.pb.h"
#include "user.grpc.pb.h"

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

void ensure_schema() {
    odb::pgsql::database db(kConnStr);
    im::test::EnsureCoreSchema(db);
}

void cleanup_users() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'grpc-user-server-test-%')");
    t.commit();
}

std::unique_ptr<im::user::UserService::Stub> make_stub(int port) {
    const std::string endpoint = "127.0.0.1:" + std::to_string(port);
    auto channel = ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials());
    return im::user::UserService::NewStub(channel);
}

}  // namespace

TEST(UserServerAppTest, RegisterLoginAndGetUserInfoThroughGrpc) {
    ensure_schema();
    cleanup_users();

    im::service::user::UserServerConfig config;
    config.listen_address = "127.0.0.1:0";
    config.postgres_connection_string = kConnStr;

    im::service::user::UserServerApp server(config);
    ASSERT_TRUE(server.start());
    ASSERT_GT(server.selected_port(), 0);

    auto stub = make_stub(server.selected_port());

    im::user::RegisterRequest register_request;
    register_request.set_account("grpc-user-server-test-flow");
    register_request.set_password("securePass123");
    register_request.set_nickname("User Server Flow");

    im::user::RegisterResponse register_response;
    ::grpc::ClientContext register_context;
    auto status = stub->Register(&register_context, register_request, &register_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(register_response.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(register_response.has_user());
    EXPECT_FALSE(register_response.user().uid().empty());
    EXPECT_EQ(register_response.user().account(), "grpc-user-server-test-flow");

    im::user::LoginRequest login_request;
    login_request.set_account("grpc-user-server-test-flow");
    login_request.set_password("securePass123");

    im::user::LoginResponse login_response;
    ::grpc::ClientContext login_context;
    status = stub->Login(&login_context, login_request, &login_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(login_response.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(login_response.has_user());
    EXPECT_EQ(login_response.user().uid(), register_response.user().uid());

    im::user::GetUserInfoRequest profile_request;
    profile_request.set_uid(register_response.user().uid());

    im::user::GetUserInfoResponse profile_response;
    ::grpc::ClientContext profile_context;
    status = stub->GetUserInfo(&profile_context, profile_request, &profile_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(profile_response.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(profile_response.has_user());
    EXPECT_EQ(profile_response.user().account(), "grpc-user-server-test-flow");
    EXPECT_EQ(profile_response.user().nickname(), "User Server Flow");

    server.shutdown();
    cleanup_users();
}

TEST(UserServerAppTest, StartReturnsFalseForInvalidListenAddress) {
    im::service::user::UserServerConfig config;
    config.listen_address = "127.0.0.1:not-a-port";
    config.postgres_connection_string = kConnStr;

    im::service::user::UserServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}

TEST(UserServerAppTest, StartReturnsFalseWithoutPostgresConnectionString) {
    im::service::user::UserServerConfig config;
    config.listen_address = "127.0.0.1:0";

    im::service::user::UserServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}
