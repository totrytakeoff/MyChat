# Current Progress

## Architecture Decision

The project keeps the original microservice direction. MVP work is now defined
per service module, not as a temporary single-process simplification.

The practical rule is:

```text
Preserve service boundaries, reduce feature breadth, finish one service MVP at
a time.
```

PostgreSQL persistence should follow the existing ODB direction. We will not
introduce a separate hand-written SQL user repository as a throwaway MVP layer
unless ODB proves unavailable or impractical after direct verification.

## Current Baseline

Known working:

- Docker Redis and PostgreSQL can be started with `docker compose`.
- Gateway builds and starts from `config/dev.json`.
- Gateway health check works on `/api/v1/health`.
- Redis hiredis wrapper has a focused test.
- Auth token primitives have a focused test.
- ODB 2.5.0 user persistence works (persist, load, erase against Docker PostgreSQL).
- User Service Core MVP: register/login/profile workflows, password hashing.
- vcpkg root is configured for `/home/myself/pkgs/vcpkg`.

Most recently verified commands:

```bash
cmake -S . -B /tmp/mychat-task4-build \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task4-build \
  --target im_user_service test_user_service test_auth_tokens \
          test_redis_hiredis test_odb_user_persistence -j2
ctest --test-dir /tmp/mychat-task4-build \
  -R 'UserServiceCoreTest|ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' \
  --output-on-failure
```

Result (ODB enabled):

```text
Tests: 100% passed, 0 tests failed out of 4
```

No-ODB baseline:

```bash
cmake -S . -B /tmp/mychat-task4-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task4-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task4-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result: Baseline green, 100% passed out of 2.

## Completed Work

- Root CMake staged options were added:
  - `MYCHAT_BUILD_TESTS`
  - `MYCHAT_BUILD_GATEWAY`
  - `MYCHAT_BUILD_SERVICES`
  - `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`
  - `MYCHAT_BUILD_PGSQL_ODB`
- vcpkg manifest was added. ODB runtime dependencies are isolated behind the
  optional `pgsql-odb` manifest feature.
- Docker Compose Redis/PostgreSQL configuration was added.
- Redis moved from stale `redis-plus-plus` usage to a hiredis-backed wrapper.
- Redis focused test was added under `test/db`.
- Auth focused test was added under `test/auth`.
- Gateway duplicate implementation was collapsed into
  `gateway/gateway_server/gateway_server.cpp`.
- Gateway can start from `config/dev.json`.
- Gateway TLS development cert paths are config-driven.
- PostgreSQL init SQL was made Docker-repeatable.
- Auth Redis token storage hardened to per-token/per-JTI keys:
  - `refresh_token:<token>` per-token metadata keys with independent TTL.
  - `revoked_access_token:<jti>` per-JTI keys with TTL aligned to token expiry.
  - `user:<user_id>:refresh_tokens` user index set.
  - Added `set`, `get`, `exists`, `ttl`, `keys` primitives to RedisClient.
  - Updated `AuthTokenTest` with 8 focused tests covering the new model.
- PostgreSQL/ODB toolchain baseline established:
  - Added `libodb` and `libodb-pgsql` behind a vcpkg feature (`pgsql-odb`).
  - Added `MYCHAT_BUILD_PGSQL_ODB` CMake option (default OFF).
  - `im_pgsql` library target compiles from `pgsql_conn.cpp`.
  - Fixed `auto`/`extern` logger conflict in `pgsql_conn.cpp`.
- ODB user persistence baseline established:
  - `services/odb/user.hpp` rewritten with valid namespaced model (manual
    string UID, password_hash, `im_users` table, `im::service::user::User`).
  - ODB generated files produced by `odb 2.5.0` compiler.
  - `im_user_odb` static library target builds behind ODB gate.
  - `ODBUserPersistenceTest` passes against Docker PostgreSQL.
- User Service Core MVP complete:
  - `im_user_service` static library target with deterministic CMakeLists.txt
    (not GLOB-based), gated on `MYCHAT_BUILD_PGSQL_ODB=ON` and `TARGET im::user_odb`.
  - `PasswordHasher` — PBKDF2-HMAC-SHA256 via OpenSSL, configurable iterations,
    constant-time verification.
  - `UserRepository` — ODB-backed CRUD for `im::service::user::User`.
  - `UserService` — register/login/profile with DTO structs (no password_hash
    in profile responses).
  - UUID v4 UID generation via `RAND_bytes` (no libuuid dependency).
  - `UserServiceCoreTest` — 5 test cases, all passing.
  - No `DROP TABLE` — cleanup by `DELETE WHERE account LIKE 'task4-test-%'`.
  - Duplicate accounts rejected explicitly (`DUPLICATE_ACCOUNT`).
  - `pgsql_conn.hpp` template issues remain unfixed; User Service bypasses
    by using `odb::pgsql::database` directly.

## In Progress

Gateway Service MVP hardening:

- Gateway handler registration needs production/service-contract cleanup.
- Old Gateway integration tests remain disabled behind
  `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`.

PostgreSQL/ODB foundation:

- ODB 2.5.0 runtime installed via `scripts/build_odb_runtime_2_5.sh`. CMake
  requires ODB 2.5.0 (detected via `MYCHAT_ODB_ROOT` or default `.odb/installed/`)
  and fails at configure time with instructions if not found. No silent fallback
  to vcpkg 2.4.0.
- ODB user persistence baseline is **reproducible**. Run `scripts/build_odb_runtime_2_5.sh`
  then `cmake ... -DMYCHAT_BUILD_PGSQL_ODB=ON`. User entity persists/loads from
  Docker PostgreSQL.

## Next Immediate Tasks

1. Gateway-to-User Service integration (Phase E) — wire Gateway auth HTTP routes
   to User Service for register/login/profile endpoints.
2. Fix `pgsql_conn.hpp` template wrapper issues (string ID handling) when
   it becomes a blocker.
3. Message Service MVP (Phase F) after Gateway integration stabilizes.

## Risks

- ODB 2.5.0 runtime must be built from source (until vcpkg packages are
  updated). Developers must run `scripts/build_odb_runtime_2_5.sh` before
  enabling ODB builds. CMake fails at configure time if runtime is missing.
- `pgsql_conn.hpp` RAII wrapper has pre-existing issues that will surface
  during User Service implementation.
- Existing `services/codec` generated files are stale. Regenerating gRPC/proto
  artifacts should be a deliberate phase, not an accidental side effect.
- Old tests still contain references to removed dependencies and may fail if
  re-enabled wholesale.
- Current Redis wrapper is single-connection and mutex-serialized. It is enough
  for correctness tests, not for performance claims.

## Documentation Index

- Architecture: `docs/architecture/mvp_architecture.md`
- Roadmap: `docs/architecture/service_mvp_roadmap.md`
- Build cleanup log: `docs/devlog/phase0_build_cleanup.md`
- Gateway/Auth baseline log: `docs/devlog/phase1_gateway_auth_baseline.md`
- Current progress: `docs/devlog/current_progress.md`
- ODB toolchain status: `docs/devlog/phase2_odb_toolchain.md`
- ODB user persistence: `docs/devlog/phase3_odb_user_persistence.md`
- User Service core: `docs/devlog/phase4_user_service_core.md`
