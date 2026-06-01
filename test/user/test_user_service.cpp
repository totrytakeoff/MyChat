#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <user.hpp>
#include <user-odb.hxx>

#include <password_hasher.hpp>
#include <user_service.hpp>

namespace {

const char* kConnStr = "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

constexpr int64_t kNowMs = 1717000000000;
constexpr int64_t kLaterMs = 1717000005000;

class UserServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        EnsureTable();
        Cleanup();
    }

    void TearDown() override {
        Cleanup();
    }

    static void EnsureTable() {
        odb::pgsql::database db(kConnStr);
        odb::transaction t(db.begin());
        db.execute(R"(
            CREATE TABLE IF NOT EXISTS "im_users" (
                "uid" TEXT NOT NULL PRIMARY KEY,
                "account" TEXT NOT NULL,
                "password_hash" TEXT NOT NULL,
                "nickname" TEXT NOT NULL,
                "avatar" TEXT NOT NULL,
                "gender" INTEGER NOT NULL,
                "signature" TEXT NOT NULL,
                "create_time" BIGINT NOT NULL,
                "last_login" BIGINT NOT NULL,
                "online" BOOLEAN NOT NULL
            )
        )");
        db.execute(R"(
            CREATE UNIQUE INDEX IF NOT EXISTS "im_users_account_i"
                ON "im_users" ("account")
        )");
        t.commit();
    }

    void Cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'task4-test-%')");
        t.commit();
    }

    std::shared_ptr<odb::pgsql::database> db_;
};

} // anonymous namespace

TEST_F(UserServiceTest, RegisterCreatesUserWithHashNotPlaintext) {
    im::service::user::UserService svc(
        db_, std::make_unique<im::service::user::PasswordHasher>());

    im::service::user::RegisterRequest req;
    req.account = "task4-test-hash-check";
    req.password = "securePass123!";
    req.now_ms = kNowMs;

    auto result = svc.register_user(req);
    ASSERT_TRUE(result.ok);
    ASSERT_FALSE(result.profile.uid.empty());
    EXPECT_EQ(result.profile.account, "task4-test-hash-check");
    EXPECT_EQ(result.profile.nickname, "task4-test-hash-check");

    // Directly query DB to verify the stored hash is NOT the plaintext password
    {
        odb::transaction t(db_->begin());
        std::unique_ptr<im::service::user::User> loaded(
            db_->load<im::service::user::User>(result.profile.uid));
        t.commit();

        ASSERT_NE(loaded, nullptr);
        EXPECT_NE(loaded->password_hash(), "securePass123!");
        EXPECT_FALSE(loaded->password_hash().empty());
        // Verify it looks like our hash format
        EXPECT_EQ(loaded->password_hash().substr(0, 13), "pbkdf2_sha256");
    }
}

TEST_F(UserServiceTest, DuplicateAccountRejected) {
    im::service::user::UserService svc(
        db_, std::make_unique<im::service::user::PasswordHasher>());

    im::service::user::RegisterRequest req;
    req.account = "task4-test-duplicate";
    req.password = "pass456";
    req.now_ms = kNowMs;

    auto first = svc.register_user(req);
    ASSERT_TRUE(first.ok);

    auto second = svc.register_user(req);
    EXPECT_FALSE(second.ok);
    EXPECT_EQ(second.error_code, "DUPLICATE_ACCOUNT");
}

TEST_F(UserServiceTest, LoginSucceedsAndUpdatesLastLogin) {
    im::service::user::UserService svc(
        db_, std::make_unique<im::service::user::PasswordHasher>());

    im::service::user::RegisterRequest req;
    req.account = "task4-test-login-ok";
    req.password = "myPassword";
    req.now_ms = kNowMs;

    auto reg = svc.register_user(req);
    ASSERT_TRUE(reg.ok);
    EXPECT_EQ(reg.profile.last_login, kNowMs);

    auto login = svc.login_by_account("task4-test-login-ok", "myPassword", kLaterMs);
    ASSERT_TRUE(login.ok);
    EXPECT_EQ(login.profile.last_login, kLaterMs);

    // Verify via direct DB that last_login was persisted
    {
        odb::transaction t(db_->begin());
        std::unique_ptr<im::service::user::User> loaded(
            db_->load<im::service::user::User>(reg.profile.uid));
        t.commit();
        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(loaded->last_login(), kLaterMs);
    }
}

TEST_F(UserServiceTest, LoginFailsWithWrongPassword) {
    im::service::user::UserService svc(
        db_, std::make_unique<im::service::user::PasswordHasher>());

    im::service::user::RegisterRequest req;
    req.account = "task4-test-login-fail";
    req.password = "correctPass";
    req.now_ms = kNowMs;

    auto reg = svc.register_user(req);
    ASSERT_TRUE(reg.ok);

    auto login = svc.login_by_account("task4-test-login-fail", "wrongPass", kLaterMs);
    EXPECT_FALSE(login.ok);
    EXPECT_EQ(login.error_code, "WRONG_PASSWORD");
}

TEST_F(UserServiceTest, ProfileLookupByUidAndAccount) {
    im::service::user::UserService svc(
        db_, std::make_unique<im::service::user::PasswordHasher>());

    im::service::user::RegisterRequest req;
    req.account = "task4-test-profile";
    req.password = "pass789";
    req.nickname = "ProfileUser";
    req.now_ms = kNowMs;

    auto reg = svc.register_user(req);
    ASSERT_TRUE(reg.ok);
    ASSERT_FALSE(reg.profile.uid.empty());

    // Lookup by UID
    auto by_uid = svc.get_profile_by_uid(reg.profile.uid);
    ASSERT_TRUE(by_uid.has_value());
    EXPECT_EQ(by_uid->account, "task4-test-profile");
    EXPECT_EQ(by_uid->nickname, "ProfileUser");

    // Lookup by account
    auto by_acct = svc.get_profile_by_account("task4-test-profile");
    ASSERT_TRUE(by_acct.has_value());
    EXPECT_EQ(by_acct->uid, reg.profile.uid);
    EXPECT_EQ(by_acct->nickname, "ProfileUser");

    // Non-existent account
    auto missing = svc.get_profile_by_account("task4-test-nonexistent");
    EXPECT_FALSE(missing.has_value());

    // Non-existent UID
    auto missing_uid = svc.get_profile_by_uid("nonexistent-uid-12345");
    EXPECT_FALSE(missing_uid.has_value());
}
