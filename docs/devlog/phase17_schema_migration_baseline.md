# Phase 17: Schema Migration Baseline

Date: 2026-06-06

## Purpose

Introduce a lightweight PostgreSQL schema migration entrypoint before broader
persistence evolution. The goal is to stop spreading ad-hoc
`CREATE TABLE IF NOT EXISTS` blocks across tests and runtime setup while
preserving the current ODB-backed schema.

This phase does not replace ODB. ODB remains the source for C++ persistence
models and generated mapping code. Migrations provide an operational way to
apply the current schema to a development database without destructive
`DROP TABLE` statements.

## Implemented State

New migration files:

- `db/migrations/001_core_schema.sql`
  - Non-destructive baseline for the current core IM tables:
    `im_users`, `im_messages`, `im_friends`, `im_groups`,
    `im_group_members`, and `im_group_messages`.
  - Mirrors the current ODB generated table shapes.
  - Uses `CREATE TABLE IF NOT EXISTS` and `CREATE UNIQUE INDEX IF NOT EXISTS`.
  - Does not drop tables or delete data.

New migration runner:

- `scripts/db/migrate_postgres.sh`
  - Creates `schema_migrations` if it does not exist.
  - Applies `db/migrations/*.sql` in filename order.
  - Records `version`, `name`, `checksum`, and `applied_at`.
  - Skips already-applied migrations with the same checksum.
  - Fails on checksum mismatch for an already-applied version.
  - Uses host `psql` when available; otherwise falls back to
    `docker compose exec -T postgres psql` for the current Docker development
    environment.

CI/local regression integration:

- `scripts/ci/remote_push_odb.sh` now applies PostgreSQL schema migrations
  after Redis/PostgreSQL dependencies are healthy and before configuring the
  ODB/gRPC build.

## Usage

Start PostgreSQL:

```bash
docker compose up -d postgres
```

Apply migrations:

```bash
scripts/db/migrate_postgres.sh
```

Useful overrides:

```bash
MYCHAT_MIGRATIONS_DIR=/path/to/migrations scripts/db/migrate_postgres.sh
MYCHAT_PGHOST=127.0.0.1 MYCHAT_PGPORT=5432 scripts/db/migrate_postgres.sh
MYCHAT_PGDATABASE=mychat MYCHAT_PGUSER=mychat MYCHAT_PGPASSWORD=mychat-dev-pass \
  scripts/db/migrate_postgres.sh
```

## Current Contract

- Migration filenames must start with a stable version prefix before the first
  underscore, for example `001_core_schema.sql`.
- Applied migration SQL must not be edited in place after it has been applied
  to a shared database. Add a new migration instead.
- The first baseline migration is idempotent and non-destructive so it can be
  applied to existing local development databases.
- Service-level User, Message, Friend, and Group tests now share
  `test/support/postgres_schema.*` instead of carrying their own table creation
  SQL blocks.
- Focused Gateway-level User, Message HTTP/WS, PushService, Friend, Group, and
  Group Message tests also use `test/support/postgres_schema.*`.
- Broader remote Push Gateway entrypoint and full GatewayServer smoke fixtures
  also use `test/support/postgres_schema.*`.
- Remaining ad-hoc test schema setup is limited to isolated low-level ODB
  persistence tests that intentionally pin a narrow single-table baseline.

## Remaining Work

- Decide whether low-level ODB persistence tests should keep their narrow
  fixture-local schema setup or move to the shared core helper later.
- Add future schema changes as new migration files instead of editing
  `001_core_schema.sql`.

## Runtime Policy

- Gateway and standalone service binaries do not run migrations implicitly.
  Runtime schema changes should remain visible operational steps.
- Local development uses an explicit pre-start hook:
  `scripts/dev/prepare_runtime.sh`.
  It starts Redis/PostgreSQL through Docker Compose when available and then
  runs `scripts/db/migrate_postgres.sh`.
- Remote Push two-process local development can use
  `scripts/dev/run_remote_push_stack.sh`, which calls the pre-start hook and
  then starts `push_server` and `gateway_server` with
  `config/dev.remote-push.json`.

## Verification

Verification on 2026-06-06:

```bash
bash -n scripts/db/migrate_postgres.sh
bash -n scripts/dev/prepare_runtime.sh scripts/dev/run_remote_push_stack.sh
jq empty config/dev.remote-push.json
scripts/ci/checks.sh
git diff --check
docker compose up -d postgres
scripts/db/migrate_postgres.sh
scripts/db/migrate_postgres.sh
cmake --build build/remote-push-odb --target \
  test_user_service test_message_service test_friend_service test_group_service \
  test_gateway_user_http test_gateway_message_http test_gateway_message_ws \
  test_push_service test_gateway_friend_http test_gateway_group_http \
  test_gateway_group_message_http test_remote_push_gateway_entrypoints \
  test_remote_push_gateway_server_smoke -j2
ctest --test-dir build/remote-push-odb \
  -R "UserServiceCoreTest|MessageServiceCoreTest|FriendServiceCoreTest|GroupServiceCoreTest" \
  --output-on-failure
ctest --test-dir build/remote-push-odb \
  -R "GatewayUserHttpTest|GatewayMessageHttpTest|GatewayMessageWsTest|PushServiceTest|GatewayFriendHttpTest|GatewayGroupHttpTest|GatewayGroupMessageHttpTest" \
  --output-on-failure
ctest --test-dir build/remote-push-odb \
  -R "RemotePushGatewayEntrypointsTest|RemotePushGatewayServerSmokeTest" \
  --output-on-failure
```

Results:

- Shell syntax check passed.
- Development runtime script syntax checks passed.
- JSON validation passed.
- Repository checks passed.
- `git diff --check` passed.
- First migration run created/used `schema_migrations` and applied
  `001_core_schema.sql`; existing development tables were skipped through
  `CREATE TABLE IF NOT EXISTS` / `CREATE INDEX IF NOT EXISTS`.
- Second migration run skipped `001_core_schema.sql` because the recorded
  checksum matched.
- Service-level User, Message, Friend, and Group tests built and passed after
  switching their table setup to `test/support/postgres_schema.*`.
- Focused Gateway-level User, Message HTTP/WS, PushService, Friend, Group, and
  Group Message tests passed after switching their table setup to
  `test/support/postgres_schema.*`.
- Remote Push Gateway entrypoint and full GatewayServer smoke fixtures now use
  `test/support/postgres_schema.*`.
