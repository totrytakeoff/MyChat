#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <friend.hpp>
#include <friend-odb.hxx>

#include <friend_service.hpp>
#include <user_service.hpp>
#include <password_hasher.hpp>

#include "../support/postgres_schema.hpp"

namespace {

using im::service::friend_::FriendService;
using im::service::friend_::FriendRequest;
using im::service::friend_::FriendResult;
using im::service::friend_::FriendInfoDTO;
using im::service::friend_::FriendStatus;
using im::service::user::UserService;
using im::service::user::PasswordHasher;
using im::service::user::RegisterRequest;

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1719000000000;

class FriendServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTables();
        Cleanup();
    }

    void TearDown() override {
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
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'friend-test-%'
                )
               OR "target_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'friend-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'friend-test-%')");
        t.commit();
    }

    std::string create_user(UserService& svc, const std::string& account) {
        RegisterRequest req;
        req.account = account;
        req.password = "testPass123";
        req.nickname = account;
        req.now_ms = kNowMs;
        auto result = svc.register_user(req);
        EXPECT_TRUE(result.ok) << "Failed to create user " << account;
        return result.profile.uid;
    }

    std::shared_ptr<odb::pgsql::database> db_;
};

TEST_F(FriendServiceTest, SendFriendRequestHappyPath) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-send-ok-a");
    std::string uid_b = create_user(*user_svc, "friend-test-send-ok-b");

    FriendRequest req;
    req.requester_uid = uid_a;
    req.target_uid = uid_b;
    req.now_ms = kNowMs;

    auto result = svc.send_request(req);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.message, "Friend request sent");
}

TEST_F(FriendServiceTest, SendFriendRequestEmptyUidRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-empty-a");

    {
        FriendRequest req;
        req.requester_uid = "";
        req.target_uid = uid_a;
        req.now_ms = kNowMs;

        auto result = svc.send_request(req);
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.error_code, "EMPTY_UID");
    }

    {
        FriendRequest req;
        req.requester_uid = uid_a;
        req.target_uid = "";
        req.now_ms = kNowMs;

        auto result = svc.send_request(req);
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.error_code, "EMPTY_UID");
    }
}

TEST_F(FriendServiceTest, SendFriendRequestSelfRequestRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-self-a");

    FriendRequest req;
    req.requester_uid = uid_a;
    req.target_uid = uid_a;
    req.now_ms = kNowMs;

    auto result = svc.send_request(req);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "SELF_REQUEST");
}

TEST_F(FriendServiceTest, SendFriendRequestTargetNotFoundRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-tnf");
    std::string nonexistent_uid = "nonexistent-uid-for-test";

    // Verify nonexistent UID doesn't resolve
    auto profile = user_svc->get_profile_by_uid(nonexistent_uid);
    EXPECT_FALSE(profile.has_value());

    FriendRequest req;
    req.requester_uid = uid_a;
    req.target_uid = nonexistent_uid;
    req.now_ms = kNowMs;

    auto result = svc.send_request(req);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "TARGET_NOT_FOUND");
}

TEST_F(FriendServiceTest, SendFriendRequestDuplicatePairRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-dup-pair-a");
    std::string uid_b = create_user(*user_svc, "friend-test-dup-pair-b");

    FriendRequest req;
    req.requester_uid = uid_a;
    req.target_uid = uid_b;
    req.now_ms = kNowMs;

    auto first = svc.send_request(req);
    EXPECT_TRUE(first.ok);

    auto second = svc.send_request(req);
    EXPECT_FALSE(second.ok);
    EXPECT_EQ(second.error_code, "ALREADY_EXISTS");
}

TEST_F(FriendServiceTest, SendFriendRequestDuplicateReverseRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-dup-rev-a");
    std::string uid_b = create_user(*user_svc, "friend-test-dup-rev-b");

    FriendRequest req;
    req.requester_uid = uid_a;
    req.target_uid = uid_b;
    req.now_ms = kNowMs;
    auto first = svc.send_request(req);
    EXPECT_TRUE(first.ok);

    FriendRequest rev_req;
    rev_req.requester_uid = uid_b;
    rev_req.target_uid = uid_a;
    rev_req.now_ms = kNowMs;
    auto second = svc.send_request(rev_req);
    EXPECT_FALSE(second.ok);
    EXPECT_EQ(second.error_code, "ALREADY_EXISTS");
}

TEST_F(FriendServiceTest, RespondToRequestOnlyTargetCanAccept) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-respond-actor-a");
    std::string uid_b = create_user(*user_svc, "friend-test-respond-actor-b");

    FriendRequest req;
    req.requester_uid = uid_a;
    req.target_uid = uid_b;
    req.now_ms = kNowMs;
    auto send_res = svc.send_request(req);
    ASSERT_TRUE(send_res.ok);

    auto pending = svc.get_pending_requests(uid_b);
    ASSERT_EQ(pending.size(), 1);
    uint64_t friend_id = pending[0].friend_id;
    ASSERT_GT(friend_id, 0);

    auto requester_resp = svc.respond_to_request(friend_id, uid_a, true);
    EXPECT_FALSE(requester_resp.ok);
    EXPECT_EQ(requester_resp.error_code, "FORBIDDEN");

    auto target_resp = svc.respond_to_request(friend_id, uid_b, true);
    EXPECT_TRUE(target_resp.ok);
    EXPECT_EQ(target_resp.message, "Friend request accepted");
}

TEST_F(FriendServiceTest, RespondToRequestNonPendingRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-nonpend-a");
    std::string uid_b = create_user(*user_svc, "friend-test-nonpend-b");

    FriendRequest req;
    req.requester_uid = uid_a;
    req.target_uid = uid_b;
    req.now_ms = kNowMs;
    auto send_res = svc.send_request(req);
    ASSERT_TRUE(send_res.ok);

    auto pending = svc.get_pending_requests(uid_b);
    ASSERT_EQ(pending.size(), 1);
    uint64_t friend_id = pending[0].friend_id;

    auto accept = svc.respond_to_request(friend_id, uid_b, true);
    ASSERT_TRUE(accept.ok);

    auto second = svc.respond_to_request(friend_id, uid_b, false);
    EXPECT_FALSE(second.ok);
    EXPECT_EQ(second.error_code, "NOT_PENDING");
}

TEST_F(FriendServiceTest, RespondToRequestNotFoundException) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    auto result = svc.respond_to_request(99999999, "some-uid", true);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "NOT_FOUND");
}

TEST_F(FriendServiceTest, GetAcceptedFriendsList) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-list-a");
    std::string uid_b = create_user(*user_svc, "friend-test-list-b");
    std::string uid_c = create_user(*user_svc, "friend-test-list-c");

    FriendRequest req_ab;
    req_ab.requester_uid = uid_a;
    req_ab.target_uid = uid_b;
    req_ab.now_ms = kNowMs;
    ASSERT_TRUE(svc.send_request(req_ab).ok);

    FriendRequest req_ac;
    req_ac.requester_uid = uid_a;
    req_ac.target_uid = uid_c;
    req_ac.now_ms = kNowMs;
    ASSERT_TRUE(svc.send_request(req_ac).ok);

    auto pending_b = svc.get_pending_requests(uid_b);
    ASSERT_EQ(pending_b.size(), 1);
    ASSERT_TRUE(svc.respond_to_request(pending_b[0].friend_id, uid_b, true).ok);

    auto pending_c = svc.get_pending_requests(uid_c);
    ASSERT_EQ(pending_c.size(), 1);
    ASSERT_TRUE(svc.respond_to_request(pending_c[0].friend_id, uid_c, true).ok);

    auto friends = svc.get_friends(uid_a);
    ASSERT_EQ(friends.size(), 2);

    bool found_b = false, found_c = false;
    for (const auto& f : friends) {
        if (f.friend_uid == uid_b) {
            found_b = true;
            EXPECT_EQ(f.status, FriendStatus::ACCEPTED);
        }
        if (f.friend_uid == uid_c) {
            found_c = true;
            EXPECT_EQ(f.status, FriendStatus::ACCEPTED);
        }
        EXPECT_FALSE(f.friend_uid.empty());
        EXPECT_FALSE(f.nickname.empty());
    }
    EXPECT_TRUE(found_b) << "uid_b should be in friend list";
    EXPECT_TRUE(found_c) << "uid_c should be in friend list";
}

TEST_F(FriendServiceTest, GetPendingRequestsList) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-pend-a");
    std::string uid_b = create_user(*user_svc, "friend-test-pend-b");
    std::string uid_c = create_user(*user_svc, "friend-test-pend-c");

    FriendRequest req_ab;
    req_ab.requester_uid = uid_a;
    req_ab.target_uid = uid_b;
    req_ab.now_ms = kNowMs;
    ASSERT_TRUE(svc.send_request(req_ab).ok);

    FriendRequest req_cb;
    req_cb.requester_uid = uid_c;
    req_cb.target_uid = uid_b;
    req_cb.now_ms = kNowMs;
    ASSERT_TRUE(svc.send_request(req_cb).ok);

    auto pending = svc.get_pending_requests(uid_b);
    ASSERT_EQ(pending.size(), 2);

    for (const auto& p : pending) {
        EXPECT_EQ(p.status, FriendStatus::PENDING);
        EXPECT_TRUE(p.friend_uid == uid_a || p.friend_uid == uid_c)
            << "Unexpected friend_uid: " << p.friend_uid;
    }

    auto pending_a = svc.get_pending_requests(uid_a);
    EXPECT_TRUE(pending_a.empty()) << "Requester should not see pending requests";

    auto pending_c = svc.get_pending_requests(uid_c);
    EXPECT_TRUE(pending_c.empty()) << "Requester should not see pending requests";
}

TEST_F(FriendServiceTest, RespondToRequestRejectUpdatesStatus) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-reject-a");
    std::string uid_b = create_user(*user_svc, "friend-test-reject-b");

    FriendRequest req;
    req.requester_uid = uid_a;
    req.target_uid = uid_b;
    req.now_ms = kNowMs;
    ASSERT_TRUE(svc.send_request(req).ok);

    auto pending = svc.get_pending_requests(uid_b);
    ASSERT_EQ(pending.size(), 1);
    uint64_t friend_id = pending[0].friend_id;

    auto reject = svc.respond_to_request(friend_id, uid_b, false);
    EXPECT_TRUE(reject.ok);
    EXPECT_EQ(reject.message, "Friend request rejected");

    auto friends = svc.get_friends(uid_a);
    EXPECT_TRUE(friends.empty()) << "Rejected request should not appear in friend list";
}

TEST_F(FriendServiceTest, GetAcceptedFriendsReturnsEmptyForNoFriends) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-nofriend-a");

    auto friends = svc.get_friends(uid_a);
    EXPECT_TRUE(friends.empty());
}

TEST_F(FriendServiceTest, GetPendingRequestsReturnsEmptyForNoRequests) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    FriendService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "friend-test-nopend-a");

    auto pending = svc.get_pending_requests(uid_a);
    EXPECT_TRUE(pending.empty());
}

} // anonymous namespace
