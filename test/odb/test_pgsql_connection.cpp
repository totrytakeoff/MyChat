#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/pgsql/database.hxx>

#include "database/pgsql/pgsql_conn.hpp"
#include "services/odb/generated/user-odb.hxx"
#include "services/odb/user.hpp"
#include "../support/postgres_schema.hpp"

namespace {

using im::db::PgSqlConnection;
using im::service::user::Gender;
using im::service::user::User;

std::string ConnStr() {
    return "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";
}

void SetupSchemaAndData(odb::pgsql::database& db) {
    im::test::EnsureCoreSchema(db);

    odb::transaction t(db.begin());
    db.execute(R"(DELETE FROM "im_users" WHERE "uid" LIKE 'test-pgsql-conn-%')");
    t.commit();
}

User MakeUser(const std::string& uid, const std::string& account) {
    return User(uid,
                account,
                "5e884898da28047151d0e56f8dc6292773603d0e56f8dc6292773603d0e56f8dc",
                "PgSqlConnTest",
                "https://example.com/pgsql_conn.png",
                Gender::OTHER,
                "wrapper test",
                3000000,
                3000000,
                false);
}

}  // namespace

TEST(PgSqlConnectionTest, SupportsStringIdCrudAndVoidTransaction) {
    auto db = std::make_shared<odb::pgsql::database>(ConnStr());
    SetupSchemaAndData(*db);

    PgSqlConnection conn(db);

    User user = MakeUser("test-pgsql-conn-001", "test_pgsql_conn_001");

    conn.execute_in_transaction([&](PgSqlConnection& tx_conn) {
        const auto persisted_id = tx_conn.persist(user);
        EXPECT_EQ(persisted_id, "test-pgsql-conn-001");
    });

    conn.execute_in_transaction([&](PgSqlConnection& tx_conn) {
        auto loaded = tx_conn.load<User>(std::string("test-pgsql-conn-001"));
        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(loaded->uid(), "test-pgsql-conn-001");
        EXPECT_EQ(loaded->account(), "test_pgsql_conn_001");
    });

    conn.execute_in_transaction([&](PgSqlConnection& tx_conn) {
        auto found = tx_conn.find<User>(std::string("test-pgsql-conn-001"));
        ASSERT_NE(found, nullptr);
        found->nickname("PgSqlConnUpdated");
        tx_conn.update(*found);
    });

    conn.execute_in_transaction([&](PgSqlConnection& tx_conn) {
        auto loaded = tx_conn.load<User>(std::string("test-pgsql-conn-001"));
        ASSERT_NE(loaded, nullptr);
        EXPECT_EQ(loaded->nickname(), "PgSqlConnUpdated");
    });

    conn.execute_in_transaction([&](PgSqlConnection& tx_conn) {
        tx_conn.erase<User>(std::string("test-pgsql-conn-001"));
    });

    conn.execute_in_transaction([&](PgSqlConnection& tx_conn) {
        auto found = tx_conn.find<User>(std::string("test-pgsql-conn-001"));
        EXPECT_EQ(found, nullptr);
    });
}

TEST(PgSqlConnectionTest, MissingStringIdErrorIsFormatted) {
    auto db = std::make_shared<odb::pgsql::database>(ConnStr());
    SetupSchemaAndData(*db);

    PgSqlConnection conn(db);

    EXPECT_THROW(
        conn.execute_in_transaction([](PgSqlConnection& tx_conn) {
            tx_conn.load<User>(std::string("test-pgsql-conn-missing"));
        }),
        std::runtime_error);
}
