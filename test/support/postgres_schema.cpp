#include "postgres_schema.hpp"

#include <odb/database.hxx>
#include <odb/transaction.hxx>

namespace im::test {

void EnsureCoreSchema(odb::database& db) {
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

    db.execute(R"(
        CREATE TABLE IF NOT EXISTS "im_messages" (
            "msg_id" BIGSERIAL NOT NULL PRIMARY KEY,
            "sender_uid" TEXT NOT NULL,
            "receiver_uid" TEXT NOT NULL,
            "content" TEXT NOT NULL,
            "msg_type" INTEGER NOT NULL,
            "status" INTEGER NOT NULL,
            "create_time" BIGINT NOT NULL,
            "delivered_time" BIGINT NOT NULL,
            "read_time" BIGINT NOT NULL
        )
    )");

    db.execute(R"(
        CREATE TABLE IF NOT EXISTS "im_friends" (
            "friend_id" BIGSERIAL NOT NULL PRIMARY KEY,
            "requester_uid" TEXT NOT NULL,
            "target_uid" TEXT NOT NULL,
            "status" INTEGER NOT NULL,
            "created_at" BIGINT NOT NULL
        )
    )");
    db.execute(R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "im_friends_pair_i"
            ON "im_friends" ("requester_uid", "target_uid")
    )");

    db.execute(R"(
        CREATE TABLE IF NOT EXISTS "im_groups" (
            "group_id" BIGSERIAL NOT NULL PRIMARY KEY,
            "name" TEXT NOT NULL,
            "creator_uid" TEXT NOT NULL,
            "created_at" BIGINT NOT NULL
        )
    )");
    db.execute(R"(
        CREATE TABLE IF NOT EXISTS "im_group_members" (
            "id" BIGSERIAL NOT NULL PRIMARY KEY,
            "group_id" BIGINT NOT NULL,
            "user_uid" TEXT NOT NULL,
            "role" INTEGER NOT NULL,
            "joined_at" BIGINT NOT NULL
        )
    )");
    db.execute(R"(
        CREATE UNIQUE INDEX IF NOT EXISTS "im_group_members_pair_i"
            ON "im_group_members" ("group_id", "user_uid")
    )");

    db.execute(R"(
        CREATE TABLE IF NOT EXISTS "im_group_messages" (
            "id" BIGSERIAL NOT NULL PRIMARY KEY,
            "group_id" BIGINT NOT NULL,
            "sender_uid" TEXT NOT NULL,
            "content" TEXT NOT NULL,
            "created_at" BIGINT NOT NULL
        )
    )");

    t.commit();
}

} // namespace im::test
