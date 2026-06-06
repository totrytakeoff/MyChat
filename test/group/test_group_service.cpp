#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <group.hpp>
#include <group-odb.hxx>

#include <group_message_service.hpp>
#include <group_service.hpp>
#include <user_service.hpp>
#include <password_hasher.hpp>

#include "../support/postgres_schema.hpp"

namespace {

using im::service::group::GroupService;
using im::service::group::GroupMessageService;
using im::service::group::CreateGroupRequest;
using im::service::group::GroupResult;
using im::service::group::GroupInfoDTO;
using im::service::group::MemberInfoDTO;
using im::service::group::GroupRole;
using im::service::user::UserService;
using im::service::user::PasswordHasher;
using im::service::user::RegisterRequest;

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1720000000000;

class GroupServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTables();
        Cleanup();
    }

    void TearDown() override {
        Cleanup();
    }

public:
    static void EnsureTables() {
        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
    }

    void Cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_group_members"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'group-test-%'
                )
               OR "user_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'group-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_groups"
            WHERE "creator_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'group-test-%'
                )
               OR "name" LIKE 'group-test-%')");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'group-test-%')");
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

TEST_F(GroupServiceTest, CreateGroupHappyPath) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid = create_user(*user_svc, "group-test-create-ok");

    CreateGroupRequest req;
    req.name = "group-test-create-ok-group";
    req.creator_uid = uid;
    req.now_ms = kNowMs;

    auto result = svc.create_group(req);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.message, "Group created");
    EXPECT_GT(result.group_id, 0);
}

TEST_F(GroupServiceTest, CreateGroupEmptyNameRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid = create_user(*user_svc, "group-test-empty-name");

    CreateGroupRequest req;
    req.name = "";
    req.creator_uid = uid;
    req.now_ms = kNowMs;

    auto result = svc.create_group(req);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "EMPTY_NAME");
}

TEST_F(GroupServiceTest, CreateGroupEmptyCreatorRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    CreateGroupRequest req;
    req.name = "group-test-group";
    req.creator_uid = "";
    req.now_ms = kNowMs;

    auto result = svc.create_group(req);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "EMPTY_CREATOR");
}

TEST_F(GroupServiceTest, CreateGroupCreatorNotFoundRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    CreateGroupRequest req;
    req.name = "group-test-group";
    req.creator_uid = "nonexistent-uid-for-group-test";
    req.now_ms = kNowMs;

    auto result = svc.create_group(req);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "CREATOR_NOT_FOUND");
}

TEST_F(GroupServiceTest, JoinGroupHappyPath) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "group-test-join-a");
    std::string uid_b = create_user(*user_svc, "group-test-join-b");

    CreateGroupRequest creq;
    creq.name = "group-test-join-group";
    creq.creator_uid = uid_a;
    creq.now_ms = kNowMs;
    auto cresult = svc.create_group(creq);
    ASSERT_TRUE(cresult.ok);
    uint64_t gid = cresult.group_id;

    auto jresult = svc.join_group(gid, uid_b, kNowMs + 1000);
    EXPECT_TRUE(jresult.ok);
    EXPECT_EQ(jresult.message, "Joined group");
}

TEST_F(GroupServiceTest, JoinGroupNotFoundRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid = create_user(*user_svc, "group-test-join-nf");

    auto result = svc.join_group(99999999, uid, kNowMs);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "GROUP_NOT_FOUND");
}

TEST_F(GroupServiceTest, JoinGroupAlreadyMemberRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "group-test-join-dup-a");
    std::string uid_b = create_user(*user_svc, "group-test-join-dup-b");

    CreateGroupRequest creq;
    creq.name = "group-test-join-dup-group";
    creq.creator_uid = uid_a;
    creq.now_ms = kNowMs;
    auto cresult = svc.create_group(creq);
    ASSERT_TRUE(cresult.ok);
    uint64_t gid = cresult.group_id;

    // uid_b joins
    auto first = svc.join_group(gid, uid_b, kNowMs);
    EXPECT_TRUE(first.ok);

    // uid_b joins again — should fail
    auto second = svc.join_group(gid, uid_b, kNowMs);
    EXPECT_FALSE(second.ok);
    EXPECT_EQ(second.error_code, "ALREADY_MEMBER");
}

TEST_F(GroupServiceTest, JoinGroupUserNotFoundRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid = create_user(*user_svc, "group-test-join-user-nf");

    CreateGroupRequest creq;
    creq.name = "group-test-join-user-nf-group";
    creq.creator_uid = uid;
    creq.now_ms = kNowMs;
    auto cresult = svc.create_group(creq);
    ASSERT_TRUE(cresult.ok);

    auto result = svc.join_group(cresult.group_id, "nonexistent-uid", kNowMs);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "USER_NOT_FOUND");
}

TEST_F(GroupServiceTest, LeaveGroupHappyPath) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "group-test-leave-a");
    std::string uid_b = create_user(*user_svc, "group-test-leave-b");

    CreateGroupRequest creq;
    creq.name = "group-test-leave-group";
    creq.creator_uid = uid_a;
    creq.now_ms = kNowMs;
    auto cresult = svc.create_group(creq);
    ASSERT_TRUE(cresult.ok);
    uint64_t gid = cresult.group_id;

    ASSERT_TRUE(svc.join_group(gid, uid_b, kNowMs).ok);

    auto lresult = svc.leave_group(gid, uid_b);
    EXPECT_TRUE(lresult.ok);
    EXPECT_EQ(lresult.message, "Left group");

    // Verify member count
    auto groups = svc.list_my_groups(uid_a);
    ASSERT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].member_count, 1);
}

TEST_F(GroupServiceTest, LeaveGroupOwnerCannotLeave) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid = create_user(*user_svc, "group-test-leave-owner");

    CreateGroupRequest creq;
    creq.name = "group-test-leave-owner-group";
    creq.creator_uid = uid;
    creq.now_ms = kNowMs;
    auto cresult = svc.create_group(creq);
    ASSERT_TRUE(cresult.ok);

    auto result = svc.leave_group(cresult.group_id, uid);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "OWNER_CANNOT_LEAVE");
}

TEST_F(GroupServiceTest, LeaveGroupNotMemberRejected) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "group-test-leave-nm-a");
    std::string uid_b = create_user(*user_svc, "group-test-leave-nm-b");

    CreateGroupRequest creq;
    creq.name = "group-test-leave-nm-group";
    creq.creator_uid = uid_a;
    creq.now_ms = kNowMs;
    auto cresult = svc.create_group(creq);
    ASSERT_TRUE(cresult.ok);

    auto result = svc.leave_group(cresult.group_id, uid_b);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "NOT_MEMBER");
}

TEST_F(GroupServiceTest, ListMyGroupsReturnsCreatedAndJoined) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "group-test-list-a");
    std::string uid_b = create_user(*user_svc, "group-test-list-b");
    std::string uid_c = create_user(*user_svc, "group-test-list-c");

    CreateGroupRequest creq;
    creq.name = "group-test-list-group-1";
    creq.creator_uid = uid_a;
    creq.now_ms = kNowMs;
    auto c1 = svc.create_group(creq);
    ASSERT_TRUE(c1.ok);

    CreateGroupRequest creq2;
    creq2.name = "group-test-list-group-2";
    creq2.creator_uid = uid_b;
    creq2.now_ms = kNowMs;
    auto c2 = svc.create_group(creq2);
    ASSERT_TRUE(c2.ok);

    ASSERT_TRUE(svc.join_group(c2.group_id, uid_a, kNowMs).ok);

    auto groups = svc.list_my_groups(uid_a);
    ASSERT_EQ(groups.size(), 2);

    bool found_g1 = false, found_g2 = false;
    for (const auto& g : groups) {
        if (g.group_id == c1.group_id) {
            found_g1 = true;
            EXPECT_EQ(g.creator_uid, uid_a);
        }
        if (g.group_id == c2.group_id) {
            found_g2 = true;
            EXPECT_EQ(g.creator_uid, uid_b);
        }
        EXPECT_GT(g.member_count, 0);
        EXPECT_FALSE(g.name.empty());
    }
    EXPECT_TRUE(found_g1);
    EXPECT_TRUE(found_g2);

    // uid_c has no groups
    auto empty = svc.list_my_groups(uid_c);
    EXPECT_TRUE(empty.empty());
}

TEST_F(GroupServiceTest, ListMembersOnlyForMembers) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid_a = create_user(*user_svc, "group-test-members-a");
    std::string uid_b = create_user(*user_svc, "group-test-members-b");
    std::string uid_c = create_user(*user_svc, "group-test-members-c");

    CreateGroupRequest creq;
    creq.name = "group-test-members-group";
    creq.creator_uid = uid_a;
    creq.now_ms = kNowMs;
    auto cresult = svc.create_group(creq);
    ASSERT_TRUE(cresult.ok);
    uint64_t gid = cresult.group_id;

    ASSERT_TRUE(svc.join_group(gid, uid_b, kNowMs).ok);

    // uid_a sees members (owner + uid_b)
    auto members_a = svc.list_members(gid, uid_a);
    ASSERT_EQ(members_a.size(), 2);

    bool found_a = false, found_b = false;
    for (const auto& m : members_a) {
        if (m.user_uid == uid_a) {
            found_a = true;
            EXPECT_EQ(m.role, GroupRole::OWNER);
        }
        if (m.user_uid == uid_b) {
            found_b = true;
            EXPECT_EQ(m.role, GroupRole::MEMBER);
        }
        EXPECT_FALSE(m.nickname.empty());
    }
    EXPECT_TRUE(found_a);
    EXPECT_TRUE(found_b);

    // uid_c (non-member) gets empty list
    auto members_c = svc.list_members(gid, uid_c);
    EXPECT_TRUE(members_c.empty());
}

TEST_F(GroupServiceTest, ListMembersGroupNotFoundReturnsEmpty) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    GroupService svc(db_, user_svc);

    std::string uid = create_user(*user_svc, "group-test-members-nf");

    auto members = svc.list_members(99999999, uid);
    EXPECT_TRUE(members.empty());
}

// ==================== GroupMessageService::send_message (service-level validation) ====================

class GroupMessageServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        GroupServiceTest::EnsureTables();
        Cleanup();
    }

    void TearDown() override {
        Cleanup();
    }

    void Cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_group_messages"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'gmsg-test-%'
                )
               OR "sender_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'gmsg-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_group_members"
            WHERE "group_id" IN (
                    SELECT "group_id" FROM "im_groups" WHERE "name" LIKE 'gmsg-test-%'
                )
               OR "user_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'gmsg-test-%'
                ))");
        db_->execute(R"(DELETE FROM "im_groups"
            WHERE "creator_uid" IN (
                    SELECT "uid" FROM "im_users" WHERE "account" LIKE 'gmsg-test-%'
                )
               OR "name" LIKE 'gmsg-test-%')");
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'gmsg-test-%')");
        t.commit();
    }

    std::string create_user(UserService& svc, const std::string& account) {
        RegisterRequest req;
        req.account = account;
        req.password = "testPass123";
        req.nickname = account;
        req.now_ms = kNowMs;
        auto result = svc.register_user(req);
        EXPECT_TRUE(result.ok);
        return result.profile.uid;
    }

    std::shared_ptr<odb::pgsql::database> db_;
};

TEST_F(GroupMessageServiceTest, SendMessageGroupNotFound) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    auto group_svc = std::make_shared<GroupService>(db_, user_svc);
    GroupMessageService svc(db_, user_svc, group_svc);

    auto result = svc.send_message(99999999, "some-user", "Hello", kNowMs);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "GROUP_NOT_FOUND");
}

TEST_F(GroupMessageServiceTest, SendMessageNonMemberForbidden) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    auto group_svc = std::make_shared<GroupService>(db_, user_svc);
    GroupMessageService svc(db_, user_svc, group_svc);

    std::string owner = create_user(*user_svc, "gmsg-test-nm-owner");
    std::string stranger = create_user(*user_svc, "gmsg-test-nm-stranger");

    CreateGroupRequest creq;
    creq.name = "gmsg-test-nm-group";
    creq.creator_uid = owner;
    creq.now_ms = kNowMs;
    auto cres = group_svc->create_group(creq);
    ASSERT_TRUE(cres.ok);

    auto result = svc.send_message(cres.group_id, stranger, "Hello", kNowMs);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "FORBIDDEN");
}

TEST_F(GroupMessageServiceTest, SendMessageMemberOk) {
    auto user_svc = std::make_shared<UserService>(db_, std::make_unique<PasswordHasher>());
    auto group_svc = std::make_shared<GroupService>(db_, user_svc);
    GroupMessageService svc(db_, user_svc, group_svc);

    std::string owner = create_user(*user_svc, "gmsg-test-ok-owner");

    CreateGroupRequest creq;
    creq.name = "gmsg-test-ok-group";
    creq.creator_uid = owner;
    creq.now_ms = kNowMs;
    auto cres = group_svc->create_group(creq);
    ASSERT_TRUE(cres.ok);

    auto result = svc.send_message(cres.group_id, owner, "Hello from owner", kNowMs);
    EXPECT_TRUE(result.ok);
    EXPECT_GT(result.msg_id, 0);
}

} // anonymous namespace
