#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <message_server_app.hpp>

#include "base.pb.h"
#include "message.grpc.pb.h"

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1718400000000;

void ensure_schema() {
    odb::pgsql::database db(kConnStr);
    im::test::EnsureCoreSchema(db);
}

void cleanup_messages() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_messages"
        WHERE "sender_uid" LIKE 'grpc-message-server-test-%'
           OR "receiver_uid" LIKE 'grpc-message-server-test-%')");
    t.commit();
}

std::unique_ptr<im::message::MessageService::Stub> make_stub(int port) {
    const std::string endpoint = "127.0.0.1:" + std::to_string(port);
    auto channel = ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials());
    return im::message::MessageService::NewStub(channel);
}

}  // namespace

TEST(MessageServerAppTest, SendPullAndMarkThroughGrpc) {
    ensure_schema();
    cleanup_messages();

    im::service::message::MessageServerConfig config;
    config.listen_address = "127.0.0.1:0";
    config.postgres_connection_string = kConnStr;

    im::service::message::MessageServerApp server(config);
    ASSERT_TRUE(server.start());
    ASSERT_GT(server.selected_port(), 0);

    auto stub = make_stub(server.selected_port());

    im::message::SendMessageRequest send_request;
    send_request.mutable_header()->set_from_uid("grpc-message-server-test-a");
    send_request.mutable_header()->set_to_uid("grpc-message-server-test-b");
    send_request.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs));
    send_request.mutable_body()->set_type(im::message::TEXT);
    send_request.mutable_body()->set_content("message server flow");

    im::message::SendMessageResponse send_response;
    ::grpc::ClientContext send_context;
    auto status = stub->SendMessage(&send_context, send_request, &send_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(send_response.base().error_code(), im::base::SUCCESS);
    ASSERT_FALSE(send_response.message().message_id().empty());
    EXPECT_EQ(send_response.message().sender_uid(), "grpc-message-server-test-a");
    EXPECT_EQ(send_response.message().receiver_uid(), "grpc-message-server-test-b");
    EXPECT_EQ(send_response.message().content(), "message server flow");

    im::message::PullOfflineRequest pull_request;
    pull_request.set_receiver_uid("grpc-message-server-test-b");
    pull_request.set_before_time(kNowMs + 5000);
    pull_request.set_limit(10);

    im::message::PullOfflineResponse pull_response;
    ::grpc::ClientContext pull_context;
    status = stub->PullOffline(&pull_context, pull_request, &pull_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    ASSERT_EQ(pull_response.base().error_code(), im::base::SUCCESS);
    ASSERT_GE(pull_response.messages_size(), 1);
    EXPECT_EQ(pull_response.messages(0).message_id(), send_response.message().message_id());

    im::message::MarkMessageDeliveredRequest mark_request;
    mark_request.set_msg_id(std::stoull(send_response.message().message_id()));
    mark_request.set_delivered_time(kNowMs + 1000);

    im::message::MarkMessageDeliveredResponse mark_response;
    ::grpc::ClientContext mark_context;
    status = stub->MarkDelivered(&mark_context, mark_request, &mark_response);

    ASSERT_TRUE(status.ok()) << status.error_message();
    EXPECT_EQ(mark_response.base().error_code(), im::base::SUCCESS);
    EXPECT_TRUE(mark_response.marked());

    server.shutdown();
    cleanup_messages();
}

TEST(MessageServerAppTest, StartReturnsFalseForInvalidListenAddress) {
    im::service::message::MessageServerConfig config;
    config.listen_address = "127.0.0.1:not-a-port";
    config.postgres_connection_string = kConnStr;

    im::service::message::MessageServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}

TEST(MessageServerAppTest, StartReturnsFalseWithoutPostgresConnectionString) {
    im::service::message::MessageServerConfig config;
    config.listen_address = "127.0.0.1:0";

    im::service::message::MessageServerApp server(config);
    EXPECT_FALSE(server.start());
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.selected_port(), 0);
}
