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
- vcpkg root is configured for `/home/myself/pkgs/vcpkg`.

Most recently verified commands:

```bash
cmake -S . -B /tmp/mychat-task2-revcheck-odb \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task2-revcheck-odb \
  --target im_pgsql test_auth_tokens test_redis_hiredis -j2
ctest --test-dir /tmp/mychat-task2-revcheck-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result (ODB enabled):

```text
Build: im_pgsql, test_auth_tokens, test_redis_hiredis
Tests: 100% passed, 0 tests failed out of 2
```

ODB-off default baseline also verified:

```bash
cmake -S . -B /tmp/mychat-task2-revcheck-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task2-revcheck-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task2-revcheck-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result: Baseline green, PgSQL targets skipped gracefully.

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

## In Progress

Gateway Service MVP hardening:

- Gateway handler registration needs production/service-contract cleanup.
- Old Gateway integration tests remain disabled behind
  `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`.

PostgreSQL/ODB foundation:

- ODB runtime libraries (libodb, libodb-pgsql) are available behind vcpkg
  feature `pgsql-odb`. To enable:
  ```bash
  cmake ... -DVCPKG_MANIFEST_FEATURES=pgsql-odb -DMYCHAT_BUILD_PGSQL_ODB=ON
  ```
- `im_pgsql` library builds with `MYCHAT_BUILD_PGSQL_ODB=ON`.
- ODB compiler (`odb`) is NOT available — blocks ODB entity compilation and
  persistence tests.
- `services/odb/user.hpp` has known issues (string `id auto`, missing
  password_hash, no namespace).

## Next Immediate Tasks

1. Install ODB compiler (`odb`) from https://www.codesynthesis.com/odb/.
2. Fix `services/odb/user.hpp` model issues.
3. Generate ODB mapping files and add minimal ODB persistence test.
4. Start User Service MVP only after ODB persistence baseline passes.

## Risks

- ODB compiler (`odb`) not installed. ODB entity build and persistence tests
  blocked until compiler is installed separately.
- `services/odb/user.hpp` has known defects (string `id auto`, missing
  password_hash, no namespace) that must be fixed before User Service MVP.
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
