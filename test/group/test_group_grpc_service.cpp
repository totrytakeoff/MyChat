#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <group_grpc_service.hpp>
#include <group_message_service.hpp>
#include <group_service.hpp>
#include <password_hasher.hpp>
#include <user_service.hpp>

#include "base.pb.h"
#include "group.grpc.pb.h"

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1721000000000;

class GroupGrpcServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        ensure_schema();
        cleanup();
        user_service_ = std::make_shared<im::service::user::UserService>(
            db_, std::make_unique<im::service::user::PasswordHasher>());
        group_service_ = std::make_shared<im::service::group::GroupService>(
            db_, user_service_);
        group_message_service_ =
            std::make_unique<im::service::group::GroupMessageService>(
                db_, user_service_, group_service_);
        grpc_ = std::make_unique<im::service::group::GroupGrpcService>(
            group_service_.get(), group_message_service_.get());
    }

    void TearDown() override {
        grpc_.reset();
        group_message_service_.reset();
        group_service_.reset();
        user_service_.reset();
        cleanup();
    }

    static void ensure_schema() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_group_messages"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'grpc-group-test-%'
                )
               OR "sender_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-group-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_group_members"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'grpc-group-test-%'
                )
               OR "user_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-group-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_groups"
            WHERE "creator_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-group-test-%'
                )
               OR "name" LIKE 'grpc-group-test-%')");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'grpc-group-test-%')");
        t.commit();
    }

    std::string create_user(const std::string& account) {
        im::service::user::RegisterRequest request;
        request.account = account;
        request.password = "securePass123";
        request.nickname = account + "-nickname";
        request.now_ms = kNowMs;
        auto result = user_service_->register_user(request);
        EXPECT_TRUE(result.ok) << result.message;
        return result.profile.uid;
    }

    im::group::CreateGroupResponse create_group(const std::string& owner,
                                                const std::string& name) {
        im::group::CreateGroupRequest request;
        request.mutable_header()->set_from_uid(owner);
        request.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs));
        request.set_name(name);

        im::group::CreateGroupResponse response;
        grpc_->CreateGroup(nullptr, &request, &response);
        return response;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::shared_ptr<im::service::group::GroupService> group_service_;
    std::unique_ptr<im::service::group::GroupMessageService> group_message_service_;
    std::unique_ptr<im::service::group::GroupGrpcService> grpc_;
};

}  // namespace

TEST_F(GroupGrpcServiceTest, CreateJoinListSendHistoryAndLeave) {
    const std::string owner = create_user("grpc-group-test-flow-owner");
    const std::string member = create_user("grpc-group-test-flow-member");

    auto create = create_group(owner, "grpc-group-test-flow");
    ASSERT_EQ(create.base().error_code(), im::base::SUCCESS);
    ASSERT_GT(create.group_id(), 0);
    EXPECT_EQ(create.group().name(), "grpc-group-test-flow");
    EXPECT_EQ(create.group().owner_uid(), owner);
    EXPECT_EQ(create.group().member_count(), 1);

    im::group::JoinGroupRequest join_request;
    join_request.mutable_header()->set_from_uid(member);
    join_request.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs + 1));
    join_request.set_group_id(create.group_id());
    im::group::GroupActionResponse join_response;
    grpc_->JoinGroup(nullptr, &join_request, &join_response);
    ASSERT_EQ(join_response.base().error_code(), im::base::SUCCESS);

    im::group::GetGroupMembersRequest members_request;
    members_request.mutable_header()->set_from_uid(owner);
    members_request.set_group_record_id(create.group_id());
    im::group::GetGroupMembersResponse members_response;
    grpc_->ListMembers(nullptr, &members_request, &members_response);
    ASSERT_EQ(members_response.base().error_code(), im::base::SUCCESS);
    ASSERT_EQ(members_response.members_size(), 2);

    im::group::SendGroupMessageRequest send_request;
    send_request.mutable_header()->set_from_uid(member);
    send_request.set_group_id(create.group_id());
    send_request.set_content("group grpc message");
    send_request.set_create_time(kNowMs + 2);
    im::group::SendGroupMessageResponse send_response;
    grpc_->SendGroupMessage(nullptr, &send_request, &send_response);
    ASSERT_EQ(send_response.base().error_code(), im::base::SUCCESS);
    ASSERT_GT(send_response.message().msg_id(), 0);
    EXPECT_EQ(send_response.message().sender_uid(), member);
    EXPECT_EQ(send_response.message().content(), "group grpc message");

    im::group::GetGroupMessagesRequest history_request;
    history_request.mutable_header()->set_from_uid(owner);
    history_request.set_group_record_id(create.group_id());
    history_request.set_since(kNowMs + 1000);
    history_request.set_limit(10);
    im::group::GetGroupMessagesResponse history_response;
    grpc_->GetGroupMessages(nullptr, &history_request, &history_response);
    ASSERT_EQ(history_response.base().error_code(), im::base::SUCCESS);
    ASSERT_EQ(history_response.messages_size(), 1);
    EXPECT_EQ(history_response.messages(0).msg_id(), send_response.message().msg_id());

    im::group::LeaveGroupRequest leave_request;
    leave_request.mutable_header()->set_from_uid(member);
    leave_request.set_group_id(create.group_id());
    im::group::GroupActionResponse leave_response;
    grpc_->LeaveGroup(nullptr, &leave_request, &leave_response);
    EXPECT_EQ(leave_response.base().error_code(), im::base::SUCCESS);
}

TEST_F(GroupGrpcServiceTest, MapsKnownErrors) {
    const std::string owner = create_user("grpc-group-test-error-owner");

    auto empty_name = create_group(owner, "");
    EXPECT_EQ(empty_name.base().error_code(), im::base::PARAM_ERROR);

    im::group::JoinGroupRequest join_missing;
    join_missing.mutable_header()->set_from_uid(owner);
    join_missing.set_group_id(99999999);
    im::group::GroupActionResponse join_missing_response;
    grpc_->JoinGroup(nullptr, &join_missing, &join_missing_response);
    EXPECT_EQ(join_missing_response.base().error_code(), im::base::NOT_FOUND);

    auto create = create_group(owner, "grpc-group-test-error-group");
    ASSERT_EQ(create.base().error_code(), im::base::SUCCESS);

    im::group::LeaveGroupRequest owner_leave;
    owner_leave.mutable_header()->set_from_uid(owner);
    owner_leave.set_group_id(create.group_id());
    im::group::GroupActionResponse owner_leave_response;
    grpc_->LeaveGroup(nullptr, &owner_leave, &owner_leave_response);
    EXPECT_EQ(owner_leave_response.base().error_code(), im::base::PERMISSION_DENIED);
}

TEST_F(GroupGrpcServiceTest, NonMemberCannotListMembersOrHistory) {
    const std::string owner = create_user("grpc-group-test-forbid-owner");
    const std::string stranger = create_user("grpc-group-test-forbid-stranger");

    auto create = create_group(owner, "grpc-group-test-forbid-group");
    ASSERT_EQ(create.base().error_code(), im::base::SUCCESS);

    im::group::GetGroupMembersRequest members_request;
    members_request.mutable_header()->set_from_uid(stranger);
    members_request.set_group_record_id(create.group_id());
    im::group::GetGroupMembersResponse members_response;
    grpc_->ListMembers(nullptr, &members_request, &members_response);
    EXPECT_EQ(members_response.base().error_code(), im::base::PERMISSION_DENIED);

    im::group::GetGroupMessagesRequest history_request;
    history_request.mutable_header()->set_from_uid(stranger);
    history_request.set_group_record_id(create.group_id());
    im::group::GetGroupMessagesResponse history_response;
    grpc_->GetGroupMessages(nullptr, &history_request, &history_response);
    EXPECT_EQ(history_response.base().error_code(), im::base::PERMISSION_DENIED);
}
