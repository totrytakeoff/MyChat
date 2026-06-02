# Task 2 Plan: PostgreSQL/ODB Toolchain Baseline

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 1 completed Gateway/Auth Redis token storage hardening. The next roadmap
step is PostgreSQL/ODB foundation work. The project already contains ODB-oriented
source and design docs:

- `common/database/pgsql/pgsql_conn.hpp`
- `common/database/pgsql/pgsql_conn.cpp`
- `common/database/pgsql/pgsql_mgr.hpp`
- `common/database/pgsql/pgsql.conn.cpp`
- `common/database/pgsql/initDB.hpp`
- `services/odb/user.hpp`
- `docs/PostgreSQL_ODB_封装设计文档.md`
- `docs/ODB_开发实践指南.md`

But the current validated build does not include PostgreSQL/ODB targets, and the
`odb` compiler was not found in the current shell during review.

This task is a foundation task. Do not implement User Service business logic
yet. First make the ODB toolchain situation explicit and, if available, bring a
minimal PostgreSQL/ODB build/test baseline online.

## Goal

Create a reliable, documented PostgreSQL/ODB foundation gate.

At the end of this task, the project should clearly answer:

1. Is the ODB compiler available?
2. Are ODB runtime headers/libraries available?
3. Can the project generate/build ODB mapping files?
4. Can a minimal ODB entity persist/load against Docker PostgreSQL?
5. If any answer is no, what exact dependency/install step is required?

## Required Investigation

Inspect the existing ODB code and identify:

- whether `common/database/pgsql/pgsql_conn.cpp` and `pgsql.conn.cpp` are
  duplicates, partial implementations, or both needed;
- whether `PgSqlManager` has corresponding `.cpp` implementation or is
  header-only;
- what generated ODB files are currently missing for `services/odb/user.hpp`;
- what CMake changes are needed to build PostgreSQL/ODB code safely behind an
  option.

Check toolchain availability:

```bash
command -v odb
odb --version
```

Check for likely runtime packages/headers:

```bash
find /usr /usr/local /home/myself/pkgs/vcpkg -path '*odb*' -maxdepth 6 2>/dev/null | sed -n '1,120p'
```

You may also inspect vcpkg package availability locally if useful, but do not
spend time fighting package installation inside this task unless it is already
available.

## Required Build Design

Add staged CMake support, but keep it non-disruptive:

- Introduce or use an option such as `MYCHAT_BUILD_PGSQL_ODB` if needed.
- Keep default behavior from breaking when ODB is unavailable.
- If ODB is unavailable, CMake should print a clear status/warning and skip ODB
  targets.
- If ODB is available, CMake should be able to generate/build the minimal ODB
  baseline.

Do not force all developers to have ODB installed just to build Gateway/Auth.

## Minimal Test Target

If ODB compiler and runtime are available:

- Add a focused test under `test/db` or `test/pgsql`.
- Use Docker PostgreSQL config from `config/dev.json` or equivalent test config.
- Persist and load one minimal ODB entity.
- Prefer a new small test entity if the existing `services/odb/user.hpp` is too
  broad or malformed, but document the reason.

If ODB is not available:

- Do not fake a passing persistence test.
- Add a configure-time/toolchain check and documentation.
- Write `task2-summary.md` with `BLOCKED_BY_TOOLCHAIN` clearly called out.

## ODB Model Review Requirements

Review `services/odb/user.hpp` before using it. Pay attention to:

- class name `user` versus likely generated table/type naming;
- `#pragma db id auto` on `std::string uid_` which may be invalid or surprising
  for ODB auto IDs;
- missing password credential fields needed later for User Service MVP;
- namespace absence;
- generated header/source naming conventions.

Do not redesign the full user entity in this task unless necessary for a
minimal compile/test. If changes are needed, keep them small and explain why.

## Documentation Requirements

Update or create:

- `docs/devlog/current_progress.md`
- `docs/devlog/phase2_odb_toolchain.md`
- optionally `docs/architecture/service_mvp_roadmap.md` if phase status changes

Document:

- detected ODB compiler/runtime status;
- exact commands run;
- whether ODB build/test is enabled or blocked;
- installation/dependency recommendation if blocked;
- next step for User Service MVP.

## Required Verification

Always run:

```bash
docker compose up -d postgres
```

If ODB is available and build support is implemented:

```bash
cmake -S . -B /tmp/mychat-task2 \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task2 -j2
ctest --test-dir /tmp/mychat-task2 -R 'PgSql|ODB|Postgres' --output-on-failure
```

Also confirm existing focused baseline still passes:

```bash
ctest --test-dir /tmp/mychat-task2 -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

If ODB is not available:

```bash
cmake -S . -B /tmp/mychat-task2-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task2-no-odb --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task2-no-odb -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

The non-ODB baseline must remain green.

## Out Of Scope

- User Service register/login implementation.
- Gateway auth endpoints.
- Message/Friend/Group service work.
- gRPC/protobuf service regeneration.
- Large rewrite of `PgSqlConnection`/`PgSqlManager` beyond what is necessary to
  establish the toolchain/build gate.
- Installing system packages unless explicitly approved by the user outside
  this task flow.

## Acceptance Criteria

The task is ready for review when:

- ODB availability has been checked and documented.
- The build does not break when ODB is missing.
- If ODB is available, a minimal ODB persistence test passes against Docker
  PostgreSQL.
- If ODB is missing, the task cleanly reports the blocker and keeps the existing
  Gateway/Auth/Redis baseline green.
- `docs/agent_context/task2-summary.md` is written using the required format.
