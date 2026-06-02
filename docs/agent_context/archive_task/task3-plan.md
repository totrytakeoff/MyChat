# Task 3 Plan: ODB User Persistence Baseline

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Toolchain Check

Planner verified the local toolchain on 2026-06-01:

```bash
command -v odb
odb --version
```

Result:

```text
/usr/bin/odb
ODB object-relational mapping (ORM) compiler for C++ 2.5.0
```

Additional verified state:

- Docker Redis/PostgreSQL are running and healthy.
- PostgreSQL accepts connections for database `mychat`, user `mychat`.
- vcpkg manifest feature `pgsql-odb` installs:
  - `libodb:x64-linux@2.4.0#12`
  - `libodb-pgsql:x64-linux@2.4.0#8`
  - `libpq:x64-linux@16.9`
- CMake configure with
  `-DVCPKG_MANIFEST_FEATURES=pgsql-odb -DMYCHAT_BUILD_PGSQL_ODB=ON`
  finds `odb::libodb` and `odb::libodb-pgsql`.

Important risk: the ODB compiler is 2.5.0 while the vcpkg runtime packages are
2.4.0. This may still work, but Task 3 must prove it by generating, compiling,
linking, and running a minimal PostgreSQL persistence test. Do not assume
compatibility just because configure succeeds.

## Context

Task 2 established the optional ODB runtime build gate, but stopped before
entity generation because the `odb` compiler was missing at that time. The
compiler is now installed, so the next task should move from runtime-only
validation to real ODB generation and PostgreSQL persistence.

Existing model:

- `services/odb/user.hpp`

Known issues:

- `#pragma db id auto` is applied to `std::string uid_`; ODB auto IDs require an
  integral type and should not be used here.
- The class and enums are in the global namespace.
- The class name is lowercase `user`.
- The model lacks `password_hash`, blocking register/login MVP.
- Architecture docs still contain stale statements about PostgreSQL/ODB status
  and already-fixed Redis token storage.

## Goal

Create the first real PostgreSQL/ODB persistence baseline for the User domain.

At the end of this task:

1. `services/odb/user.hpp` has a valid, namespaced user entity model.
2. ODB generated files for the user model are produced by the documented
   `odb` command.
3. Generated files are built through CMake behind the existing ODB gate.
4. A focused test persists and loads one user row from Docker PostgreSQL.
5. Docs record the exact generation/build/test flow and any version warnings.

## Required Model Design

Use this conservative shape unless there is a concrete ODB blocker:

- Namespace: `im::service::user`
- Entity class: `User`
- Table name: choose an explicit non-reserved table name such as `im_users`
  using an ODB pragma.
- Primary key: manual string UID, not auto-generated:

```cpp
#pragma db id
std::string uid_;
```

- Required fields:
  - `uid`
  - `account`
  - `password_hash`
  - `nickname`
  - `avatar`
  - `gender`
  - `signature`
  - `create_time`
  - `last_login`
  - `online`

Optional contact/profile fields may remain if they do not complicate ODB
generation. Do not expand the model beyond current scope.

Document in a short code comment why UID is manual and why only a password hash
is stored.

## Required ODB Generation

Generate PostgreSQL ODB files from `services/odb/user.hpp`.

Preferred generated-file location:

```text
services/odb/generated/
```

Preferred generated files:

```text
services/odb/generated/user-odb.hxx
services/odb/generated/user-odb.ixx
services/odb/generated/user-odb.cxx
services/odb/generated/user.sql
```

Use an explicit command similar to:

```bash
mkdir -p services/odb/generated
odb \
  --database pgsql \
  --std c++20 \
  --generate-query \
  --generate-schema \
  --schema-format sql \
  --output-dir services/odb/generated \
  --hxx-suffix .hxx \
  --ixx-suffix .ixx \
  --cxx-suffix .cxx \
  --sql-suffix .sql \
  services/odb/user.hpp
```

If this exact command fails, adjust it minimally and document the final working
command in `docs/devlog/phase3_odb_user_persistence.md`.

Do not manually edit generated ODB files except to recover from a generator
bug, and only if documented clearly. Generated files should be treated as
mechanical artifacts.

## Required Build Design

Add a CMake target for the generated user ODB model behind:

```cmake
if(MYCHAT_BUILD_PGSQL_ODB AND MYCHAT_HAS_ODB_RUNTIME)
```

Expected target shape:

- static library target, e.g. `im_user_odb`
- source:
  - `services/odb/generated/user-odb.cxx`
- include directories:
  - `services/odb`
  - `services/odb/generated`
- link:
  - `odb::libodb`
  - `odb::libodb-pgsql`

Expose an alias if useful, e.g. `im::user_odb`.

If generated files are absent and `MYCHAT_BUILD_PGSQL_ODB=ON`, CMake should
fail with a clear message telling the developer to run the documented `odb`
generation command. Silent skipping is not acceptable once this task owns the
generated baseline.

## Required Persistence Test

Add a focused test under `test/odb` or `test/pgsql`.

Test name:

```text
ODBUserPersistenceTest
```

The test should:

- connect to Docker PostgreSQL using the dev/test config values:
  - host `127.0.0.1`
  - port `5432`
  - database `mychat`
  - user `mychat`
  - password `mychat-dev-pass`
- create or reset the generated user table for the test;
- persist one `User`;
- load it by UID;
- verify account, password hash, nickname, and at least one profile/status
  field;
- clean up inserted test data;
- be deterministic when run repeatedly.

Prefer using the generated `user.sql` schema for setup if practical. If direct
SQL is needed for cleanup/setup, keep it narrow and document why.

Do not implement register/login service logic in this test. This is a storage
baseline, not User Service MVP behavior.

## Documentation Requirements

Update or create:

- `docs/devlog/phase3_odb_user_persistence.md`
- `docs/devlog/current_progress.md`
- `docs/architecture/mvp_architecture.md`
- `docs/architecture/service_mvp_roadmap.md`
- `docs/agent_context/task3-summary.md`

Docs must include:

- exact `odb --version`;
- exact generation command;
- generated files list;
- CMake configure/build/test commands;
- note about ODB compiler 2.5.0 vs runtime 2.4.0, and whether the generated
  persistence test proves compatibility;
- remaining boundary before User Service MVP starts.

Architecture docs currently have stale claims:

- PostgreSQL/ODB is not part of the validated build/test baseline;
- refresh-token/access-revocation Redis work is still pending.

Correct them:

- ODB runtime baseline is complete behind `pgsql-odb`;
- after this task, user ODB persistence baseline should be marked complete if
  the test passes;
- Auth Redis token storage hardening is already complete;
- User Service business logic remains not implemented.

## Required Verification

Start dependencies:

```bash
docker compose up -d redis postgres
docker compose exec -T postgres pg_isready -U mychat -d mychat
docker compose exec -T redis redis-cli -a mychat-dev-pass ping
```

ODB toolchain:

```bash
odb --version
```

Generate ODB files:

```bash
rm -rf services/odb/generated
mkdir -p services/odb/generated
# run the final documented odb command
```

ODB-enabled configure/build/test:

```bash
cmake -S . -B /tmp/mychat-task3 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3 \
  --target im_pgsql im_user_odb test_odb_user_persistence test_auth_tokens test_redis_hiredis -j2
ctest --test-dir /tmp/mychat-task3 \
  -R 'ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Default no-ODB baseline:

```bash
cmake -S . -B /tmp/mychat-task3-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task3-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

If `/tmp` quota becomes an issue, clean old `/tmp/mychat-*` build directories
and rerun the same commands. Document any environment-only failure separately
from code/test failures.

## Out Of Scope

- User Service register/login implementation.
- Password hashing algorithm implementation.
- Gateway-to-User integration.
- Full schema migration framework.
- Message/Friend/Group persistence models.
- Broad cleanup of old tests unrelated to ODB user persistence.

## Acceptance Criteria

The task is ready for review when:

- User model is valid, namespaced, and includes `password_hash`.
- ODB generated files are present and reproducible with a documented command.
- CMake builds the generated user ODB target behind the ODB gate.
- `ODBUserPersistenceTest` passes against Docker PostgreSQL.
- Existing Auth/Redis focused tests still pass.
- Default no-ODB Gateway/Auth baseline still passes.
- Docs accurately reflect the new ODB toolchain and persistence status.
- `docs/agent_context/task3-summary.md` is written with files changed,
  behavior changed, verification results, known limitations, and deviations.
