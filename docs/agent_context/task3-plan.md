# Task 3 Plan: ODB User Model Cleanup and Documentation Sync

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 2 approved the optional PostgreSQL/ODB runtime baseline:

- default Gateway/Auth builds do not install ODB runtime packages;
- `-DVCPKG_MANIFEST_FEATURES=pgsql-odb -DMYCHAT_BUILD_PGSQL_ODB=ON` enables
  `im_pgsql`;
- the `odb` compiler binary is still missing, so generated ODB mapping files
  and real persistence tests remain blocked.

Before starting User Service business logic, clean the user persistent model so
it is ready for ODB generation once the compiler is installed.

Existing model:

- `services/odb/user.hpp`

Known issues:

- `#pragma db id auto` is applied to `std::string uid_`; ODB auto IDs require an
  integral type, so this is invalid/surprising.
- The class and enums are in the global namespace.
- The model has no password hash field, which blocks register/login MVP.
- The public type name is lowercase `user`, which is awkward for project code
  and can collide with PostgreSQL/user terminology.
- Architecture docs still contain stale status about Redis token storage and
  PostgreSQL/ODB validation.

## Goal

Make the user ODB model structurally ready for User Service MVP without
pretending persistence is complete.

At the end of this task:

1. The user model should be namespaced and compile as a normal C++ header.
2. The primary key should be a manual string UID or another explicitly chosen
   valid design, not `string id auto`.
3. Password hash storage needed by register/login should exist.
4. A focused compile-level test should verify the model API without requiring
   the `odb` compiler.
5. Architecture/progress docs should match the current Task 2 baseline.

## Required Design

Prefer this conservative model direction unless you find a concrete blocker:

- Namespace: `im::service::user` or `im::services::user`.
- Entity name: `User`.
- Primary key: manual string UID:

```cpp
#pragma db id
std::string uid_;
```

- Required fields for User Service MVP:
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
  - optional contact/profile fields may remain, but do not expand the model.

Keep comments useful but not noisy. Comment the persistence decisions that are
easy to misuse later, especially manual UID and password hash.

Do not generate ODB mapping files in this task unless the `odb` compiler is
actually available. If it is unavailable, document the blocker and keep the test
compile-only.

## Required Implementation

1. Update `services/odb/user.hpp`.
2. Add a focused test under `test/odb` or another clearly named test directory.
   The test should:
   - include `services/odb/user.hpp`;
   - construct a `User`;
   - verify UID/account/password hash/nickname/profile getters/setters;
   - verify enum use;
   - not connect to PostgreSQL;
   - not require generated `*-odb.*` files.
3. Wire the test into CMake only when ODB runtime headers are available:
   - likely behind `MYCHAT_BUILD_PGSQL_ODB AND MYCHAT_HAS_ODB_RUNTIME`;
   - use the existing `pgsql-odb` vcpkg feature path.
4. Update stale docs:
   - `docs/architecture/mvp_architecture.md`
   - `docs/architecture/service_mvp_roadmap.md`
   - `docs/devlog/current_progress.md`
   - optionally add `docs/devlog/phase3_odb_user_model.md`
5. Write `docs/agent_context/task3-summary.md`.

## Documentation Corrections Required

`docs/architecture/mvp_architecture.md` currently has stale statements:

- it says PostgreSQL/ODB is not part of the validated build/test baseline;
- it lists refresh-token/access-revocation Redis work as pending even though
  Task 1 completed it.

Update it to say:

- PostgreSQL/ODB runtime baseline is validated behind the optional `pgsql-odb`
  feature;
- ODB compiler and generated mapping/persistence tests remain blocked;
- Auth Redis token storage has been hardened;
- User Service MVP is still not implemented.

`docs/architecture/service_mvp_roadmap.md` Phase C should be updated from "not
started" to "runtime baseline complete; ODB compiler/persistence test blocked".

## Required Verification

Always start Docker dependencies first, even though this task's new test should
not use PostgreSQL directly:

```bash
docker compose up -d redis postgres
```

ODB-runtime enabled configure/build/test:

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
  --target im_pgsql test_odb_user_model test_auth_tokens test_redis_hiredis -j2
ctest --test-dir /tmp/mychat-task3 \
  -R 'ODBUserModelTest|AuthTokenTest|RedisHiredisTest' --output-on-failure
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

Also check:

```bash
command -v odb
odb --version
```

If the compiler is still absent, record that in the summary and docs.

## Out Of Scope

- User Service register/login implementation.
- Password hashing implementation.
- PostgreSQL schema migration.
- Generated ODB files, unless `odb` is available and generation succeeds
  without expanding scope.
- Gateway-to-User integration.
- Broad cleanup of old tests unrelated to this model.

## Acceptance Criteria

The task is ready for review when:

- `services/odb/user.hpp` has a valid, namespaced model shape for later ODB
  generation.
- A compile-level model test passes when ODB runtime is enabled.
- Default no-ODB Gateway/Auth baseline still passes.
- Docs no longer contradict Task 1 and Task 2 outcomes.
- `docs/agent_context/task3-summary.md` is written with files changed,
  behavior changed, verification results, known limitations, and deviations.
