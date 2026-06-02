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
- Message Service persistence core works: validated one-to-one text send,
  chronological conversation history, offline pull, and delivered/read marking
  against ODB/PostgreSQL.
- vcpkg root is configured for `/home/myself/pkgs/vcpkg`.

Most recently verified commands:

```bash
docker compose up -d redis postgres
cmake -S . -B /tmp/mychat-build-task003 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task003 -j2
ctest --test-dir /tmp/mychat-build-task003 --output-on-failure
```

Result (ODB enabled): 100% passed, 0 failed out of 6.

No-ODB baseline:

```bash
cmake -S . -B /tmp/mychat-build-task003-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task003-noodb -j2
```

Result: Build succeeded; Message Service targets were skipped because ODB was
not enabled.

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
- Build gating and test hygiene:
  - Added `MYCHAT_BUILD_USER_SERVICE` (ON by default), `MYCHAT_BUILD_CODEC_SERVICE`
    (OFF by default), `MYCHAT_BUILD_LEGACY_UNIT_TESTS` (OFF by default).
  - Codec service properly gated with gRPC dependency detection; FATAL_ERROR if
    explicitly enabled but deps missing.
  - User service gated on `MYCHAT_BUILD_USER_SERVICE AND TARGET im::user_odb`.
  - Legacy tests (router, utils, network) gated behind `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.
  - Full `cmake --build ... -j2` now succeeds without stale codec failures.
  - Fresh `ctest` only registers buildable active baseline tests (5 with ODB,
    2 without).
- Gateway-to-User Service HTTP Integration (Phase 6):
  - `UserHttpController` with handle_register, handle_login, handle_profile.
  - Route paths: POST `/api/v1/auth/register`, POST `/api/v1/auth/login`,
    GET `/api/v1/auth/info`.
  - Route registration extracted to a free function `register_user_http_routes_on_server`,
    called after controller creation and before legacy catch-all handlers.
  - CMake target order fixed: `services` before `gateway` in root CMakeLists.txt.
  - 10 tests: 9 controller-layer + 1 HTTP route-layer integration test.
  - Full baseline: 5/5 ODB-enabled, 2/2 no-ODB.
- Message Service persistence core (Task 003):
  - Added `MYCHAT_BUILD_MESSAGE_SERVICE` option and staged Message Service
    targets gated on `im::message_odb`.
  - Added `services/odb/message.hpp`, ODB-generated message mapping files, and
    `im_message_odb` for the `im_messages` table.
  - Added `im_message_service` with `MessageRepository` and `MessageService`.
  - Supports validated direct text send, conversation history,
    pending-offline pull, delivered marking, and read marking.
  - Conversation and offline queries use explicit `ORDER BY create_time`.
  - `MessageServiceCoreTest` covers send, validation, ordering, offline pull,
    delivered/read marking, and nonexistent-message rejection.
  - Full ODB-enabled baseline: 6/6 suites passed. No-ODB build skips Message
    Service targets cleanly.

## In Progress

- Message Service MVP (Phase F) is in progress. Persistence core is complete;
  Gateway online delivery and Push fanout remain.

## Next Immediate Tasks

1. Gateway online delivery for Message Service via HTTP/WebSocket and
   `ConnectionManager`/Push path.
2. Decide whether Gateway-to-Message should use the same direct integration
   pattern first or require codec/gRPC regeneration.
3. Fix `pgsql_conn.hpp` template wrapper issues (string ID handling) when
   it becomes a blocker.

## Risks

- ODB 2.5.0 runtime must be built from source (until vcpkg packages are
  updated). Developers must run `scripts/build_odb_runtime_2_5.sh` before
  enabling ODB builds. CMake fails at configure time if runtime is missing.
- `pgsql_conn.hpp` RAII wrapper has pre-existing issues (string-ID handling,
  raw-pointer return). User Service bypasses it by using `odb::pgsql::database`
  directly; issues will surface during Friend/Group service development.
- Existing `services/codec` generated files are stale. Regenerating gRPC/proto
  artifacts should be a deliberate phase, not an accidental side effect.
- Old tests still contain references to removed dependencies and may fail if
  re-enabled wholesale.
- Current Redis wrapper is single-connection and mutex-serialized. It is enough
  for correctness tests, not for performance claims.
- Full Phase F is not complete: Gateway online delivery, Push fanout,
  codec/gRPC decisions, and schema migration remain future work.
- `SendRequest::msg_type` is caller-supplied even though the method is named
  `send_text_message`; defaulting it to `MessageType::TEXT` is a future cleanup.

## Documentation Index

- Architecture: `docs/architecture/mvp_architecture.md`
- Roadmap: `docs/architecture/service_mvp_roadmap.md`
- Build cleanup log: `docs/devlog/phase0_build_cleanup.md`
- Gateway/Auth baseline log: `docs/devlog/phase1_gateway_auth_baseline.md`
- Current progress: `docs/devlog/current_progress.md`
- ODB toolchain status: `docs/devlog/phase2_odb_toolchain.md`
- ODB user persistence: `docs/devlog/phase3_odb_user_persistence.md`
- User Service core: `docs/devlog/phase4_user_service_core.md`
- Build gating and test hygiene: `docs/devlog/phase5_build_gating.md`
- Gateway-user HTTP integration: `docs/devlog/phase6_gateway_user_integration.md`
- Message Service persistence core: `docs/devlog/phase6_message_service_core.md`
- Agent context: `docs/agent_context/project_context.md`, `architecture_analysis.md`, `roadmap.md`, `todo.md`
- Codgent task001 final record: `docs/agent_context/tasks/task001/final.md`
- Codgent task003 final record: `docs/agent_context/tasks/task003/final.md`
