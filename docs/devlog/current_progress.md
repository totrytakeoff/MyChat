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
cmake -S . -B /tmp/mychat-task1 \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task1 --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task1 -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 2
```

Gateway smoke result:

```text
/tmp/mychat-task1/gateway/gateway_server --config config/dev.json
curl http://127.0.0.1:8102/api/v1/health
{"status": "ok"}
SIGINT shutdown -> exit code 0
```

## Completed Work

- Root CMake staged options were added:
  - `MYCHAT_BUILD_TESTS`
  - `MYCHAT_BUILD_GATEWAY`
  - `MYCHAT_BUILD_SERVICES`
  - `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`
- vcpkg manifest was added.
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

## In Progress

Gateway Service MVP hardening:

- Gateway handler registration needs production/service-contract cleanup.
- Old Gateway integration tests remain disabled behind
  `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`.

PostgreSQL/ODB foundation:

- ODB design documents and source wrappers exist.
- ODB is not yet validated in the current build/test baseline.
- `odb` compiler was not found in the current shell during this review.

## Next Immediate Tasks

1. Verify and install/document ODB toolchain:
   - confirm package source;
   - add CMake detection;
   - document generation command.
3. Bring PgSQL/ODB infrastructure into the staged build.
4. Add a minimal ODB persistence integration test against Docker PostgreSQL.
5. Start User Service MVP only after ODB persistence baseline passes.

## Risks

- ODB may not be available through the current vcpkg manifest. If it must be
  installed through the system package manager, that dependency needs explicit
  documentation and a configure-time check.
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
