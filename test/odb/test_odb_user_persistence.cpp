#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include "services/odb/user.hpp"
#include "services/odb/generated/user-odb.hxx"
#include "../support/postgres_schema.hpp"

namespace {

const char* kTestUid = "test-uid-001";
const char* kTestAccount = "testuser";
const char* kTestPasswordHash = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
const char* kTestNickname = "TestUser";
const char* kTestAvatar = "https://example.com/avatar.png";
const char* kTestSignature = "Hello, MyChat!";

// Build a libpq connection string for the dev/test Docker PostgreSQL.
std::string ConnStr() {
    return "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";
}

void SetupSchemaAndData(odb::pgsql::database& db) {
    im::test::EnsureCoreSchema(db);

    // Clean up any previously inserted test rows without touching user data.
    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_users" WHERE "uid" LIKE 'test-%')");
    t.commit();
}

} // anonymous namespace

TEST(ODBUserPersistenceTest, PersistAndLoadByUid) {
    odb::pgsql::database db(ConnStr());
    SetupSchemaAndData(db);

    im::service::user::User user(
        kTestUid,
        kTestAccount,
        kTestPasswordHash,
        kTestNickname,
        kTestAvatar,
        im::service::user::Gender::MALE,
        kTestSignature,
        1000000,
        1000000,
        false
    );

    // Persist
    {
        odb::transaction t(db.begin());
        db.persist(user);
        t.commit();
    }

    // Load by UID and verify
    {
        odb::transaction t(db.begin());
        std::unique_ptr<im::service::user::User> loaded(
            db.load<im::service::user::User>(std::string(kTestUid)));
        t.commit();

        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(loaded->uid(), kTestUid);
        EXPECT_EQ(loaded->account(), kTestAccount);
        EXPECT_EQ(loaded->password_hash(), kTestPasswordHash);
        EXPECT_EQ(loaded->nickname(), kTestNickname);
        EXPECT_EQ(loaded->avatar(), kTestAvatar);
        EXPECT_EQ(loaded->gender(), im::service::user::Gender::MALE);
        EXPECT_EQ(loaded->signature(), kTestSignature);
        EXPECT_EQ(loaded->create_time(), 1000000);
        EXPECT_EQ(loaded->last_login(), 1000000);
        EXPECT_FALSE(loaded->online());
    }

    // Cleanup
    {
        odb::transaction t(db.begin());
        db.erase<im::service::user::User>(std::string(kTestUid));
        t.commit();
    }
}

TEST(ODBUserPersistenceTest, DeterministicReRun) {
    odb::pgsql::database db(ConnStr());
    SetupSchemaAndData(db);

    im::service::user::User user(
        "test-uid-002",
        "testuser2",
        kTestPasswordHash,
        kTestNickname,
        kTestAvatar,
        im::service::user::Gender::FEMALE,
        "rerun test",
        2000000,
        2000000,
        true
    );

    {
        odb::transaction t(db.begin());
        db.persist(user);
        t.commit();
    }

    {
        odb::transaction t(db.begin());
        std::unique_ptr<im::service::user::User> loaded(
            db.load<im::service::user::User>(std::string("test-uid-002")));
        t.commit();

        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(loaded->account(), "testuser2");
        EXPECT_TRUE(loaded->online());
    }

    {
        odb::transaction t(db.begin());
        db.erase<im::service::user::User>(std::string("test-uid-002"));
        t.commit();
    }
}
