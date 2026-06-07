#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <friend_grpc_service.hpp>
#include <friend_service.hpp>
#include <password_hasher.hpp>
#include <user_service.hpp>

#include "base.pb.h"
#include "friend.grpc.pb.h"

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1718400000000;

class FriendGrpcServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        ensure_schema();
        cleanup();
        user_service_ = std::make_shared<im::service::user::UserService>(
            db_, std::make_unique<im::service::user::PasswordHasher>());
        friend_service_ = std::make_unique<im::service::friend_::FriendService>(
            db_, user_service_);
        grpc_ = std::make_unique<im::service::friend_::FriendGrpcService>(
            friend_service_.get());
    }

    void TearDown() override {
        grpc_.reset();
        friend_service_.reset();
        user_service_.reset();
        cleanup();
    }

    static void ensure_schema() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_friends"
            WHERE "requester_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-friend-test-%'
                )
               OR "target_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'grpc-friend-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'grpc-friend-test-%')");
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

    im::friend_::AddFriendResponse send_request(const std::string& requester,
                                                const std::string& target) {
        im::friend_::AddFriendRequest request;
        request.mutable_header()->set_from_uid(requester);
        request.mutable_header()->set_timestamp(static_cast<uint64_t>(kNowMs));
        request.set_to_uid(target);

        im::friend_::AddFriendResponse response;
        grpc_->SendRequest(nullptr, &request, &response);
        return response;
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::unique_ptr<im::service::friend_::FriendService> friend_service_;
    std::unique_ptr<im::service::friend_::FriendGrpcService> grpc_;
};

}  // namespace

TEST_F(FriendGrpcServiceTest, SendPendingRespondAndListFriends) {
    const std::string uid_a = create_user("grpc-friend-test-flow-a");
    const std::string uid_b = create_user("grpc-friend-test-flow-b");

    auto send = send_request(uid_a, uid_b);
    ASSERT_EQ(send.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(send.has_request());
    EXPECT_EQ(send.request().from_uid(), uid_a);
    EXPECT_EQ(send.request().to_uid(), uid_b);
    EXPECT_EQ(send.request().status(), im::friend_::PENDING);
    EXPECT_GT(send.request().friend_id(), 0);

    im::friend_::GetFriendRequestsRequest pending_request;
    pending_request.mutable_header()->set_from_uid(uid_b);
    im::friend_::GetFriendRequestsResponse pending_response;
    grpc_->GetPendingRequests(nullptr, &pending_request, &pending_response);

    ASSERT_EQ(pending_response.base().error_code(), im::base::SUCCESS);
    ASSERT_EQ(pending_response.requests_size(), 1);
    EXPECT_EQ(pending_response.requests(0).from_uid(), uid_a);
    EXPECT_EQ(pending_response.requests(0).to_uid(), uid_b);

    im::friend_::HandleFriendRequest accept_request;
    accept_request.mutable_header()->set_from_uid(uid_b);
    accept_request.set_request_id(send.request().request_id());
    accept_request.set_accept(true);
    im::friend_::HandleFriendResponse accept_response;
    grpc_->RespondToRequest(nullptr, &accept_request, &accept_response);
    ASSERT_EQ(accept_response.base().error_code(), im::base::SUCCESS);

    im::friend_::GetFriendListRequest list_request;
    list_request.mutable_header()->set_from_uid(uid_a);
    im::friend_::GetFriendListResponse list_response;
    grpc_->GetFriends(nullptr, &list_request, &list_response);

    ASSERT_EQ(list_response.base().error_code(), im::base::SUCCESS);
    ASSERT_EQ(list_response.friends_size(), 1);
    EXPECT_EQ(list_response.friends(0).uid(), uid_b);
    EXPECT_EQ(list_response.friends(0).status(), im::friend_::ACCEPTED);
    EXPECT_FALSE(list_response.friends(0).nickname().empty());
}

TEST_F(FriendGrpcServiceTest, SendRequestMapsKnownErrors) {
    const std::string uid_a = create_user("grpc-friend-test-error-a");

    auto self = send_request(uid_a, uid_a);
    EXPECT_EQ(self.base().error_code(), im::base::PARAM_ERROR);

    auto missing = send_request(uid_a, "grpc-friend-test-missing-target");
    EXPECT_EQ(missing.base().error_code(), im::base::NOT_FOUND);
}

TEST_F(FriendGrpcServiceTest, RespondRejectsInvalidRequestId) {
    im::friend_::HandleFriendRequest request;
    request.mutable_header()->set_from_uid("grpc-friend-test-actor");
    request.set_request_id("not-a-number");
    request.set_accept(true);

    im::friend_::HandleFriendResponse response;
    grpc_->RespondToRequest(nullptr, &request, &response);

    EXPECT_EQ(response.base().error_code(), im::base::PARAM_ERROR);
}
