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
- Gateway Message HTTP integration works: authenticated send, conversation
  history, and offline pull routes backed by Message Service. The token UID is
  the trusted sender/actor, and offline pull marks returned messages delivered.
- Gateway WebSocket message send/ack works: `CMD_SEND_MESSAGE` handler parses
  `SendMessageRequest`, persists through `MessageService`, returns encoded
  `SendMessageResponse` protobuf. Token-derived sender identity, protobuf type
  validation, cmd_id validation, and protobuf error responses for all expected
  failure paths.
- Gateway WebSocket online message delivery works: after successful persistence,
  `PushService::push_to_user` pushes a `CMD_PUSH_MESSAGE` +
  `im.push.PushRequest` to the recipient's active WebSocket sessions via
  `ConnectionManager` and `WebSocketServer`, and marks the message delivered
  after at least one successful push. Best-effort: messages to offline users
  stay undelivered and are pullable via the offline-pull API.
  `PushService` has a pluggable `FanoutPolicy` abstraction; the default
  `AllSessionsFanoutPolicy` pushes to all active sessions. `MessageWsHandler`
  delegates to `PushService` instead of owning the push loop directly.
- FanoutPolicy production implementations added: `PlatformFilterFanoutPolicy`
  selects sessions matching a set of allowed platforms (e.g., mobile-only);
  `NewestSessionFanoutPolicy` selects the single most recently connected
  session. Fanout policy logic now lives in `services/push/fanout_policy.*`
  and is linked through `im::push_service`. `PushServiceTest` covers all
  policies.
- Gateway source layout cleanup completed: REST controllers now live under
  `gateway/http`, push/fanout components under `gateway/push`, and WebSocket
  message handling under `gateway/ws`. Tracked clangd cache artifacts were
  removed and local IDE/index cache patterns were added to `.gitignore`.
- Push Service boundary work completed: `services/push` now provides the
  `im::service::push::PushNotifier` interface, service-owned fanout policies,
  and `im::push_service` CMake target. Gateway WS and Group Message HTTP call
  sites depend on `PushNotifier*`; the current in-process
  `gateway/push/PushService` implements that boundary and adapts Gateway
  session data into service-owned `PushSessionInfo` values.
- Push runtime core extraction completed: `services/push/PushRuntime` owns
  fanout selection, protobuf push payload construction, best-effort send
  orchestration, and delivered marking after at least one successful send.
  Gateway `PushService` is now a thin adapter for `ConnectionManager`,
  `WebSocketServer`, and Message Service delivered marking.
- Gateway Push/WS build gating was tightened after the boundary move:
  `PushService`, `MessageWsHandler`, and Group Message HTTP fanout now require
  both the relevant service targets and `im::push_service`. Message HTTP stays
  independently gated on Message Service. A `MYCHAT_BUILD_SERVICES=OFF`
  Gateway-only configure/build was added to the verification set.
- Push behavior characterization tests were added for the migration boundary:
  successful WebSocket direct-message sends notify the receiver through
  `PushNotifier`, and group-message sends fan out to other members only, not
  the sender.
- Codec/gRPC generation chain cleanup completed after the Gateway layout
  cleanup: `common/proto` is the canonical proto source tree,
  `generate_common_proto` regenerates protobuf outputs, `generate_codec_grpc`
  remains as the legacy-compatible codec target, `generate_push_grpc`
  regenerates `common/proto/push.grpc.pb.*`, and aggregate `generate_proto`
  covers active protobuf/gRPC generation when the gRPC plugin is available.
  `im::proto` now includes `codec_service.pb.cc`; `im_codec_service` consumes
  canonical generated files from `common/proto` and no longer compiles
  duplicated `services/codec/base.pb.cc`.
- Push gRPC remote boundary introduced: `common/proto/push.proto` now defines
  `im.push.PushService.NotifyUser`, `NotifyUserRequest`, and
  `NotifyUserResponse`; `common/proto/push.pb.*` and
  `common/proto/push.grpc.pb.*` were regenerated through CMake
  `generate_proto`. `services/push/PushGrpcService` adapts the generated gRPC
  service to the existing `PushNotifier` boundary. `im::push_grpc_service` is
  gated by `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`, so default builds still do not
  require gRPC.
- Gateway remote PushNotifier wiring is now available behind explicit gRPC
  builds: `gateway/push/RemotePushNotifier` uses the generated
  `im.push.PushService::Stub`, `GatewayServer` selects local vs. remote through
  `push.mode`, and `config/dev.json` defaults to `push.mode = "local"` so
  existing in-process behavior remains the default.
- Standalone Push server process slice is now available behind explicit gRPC
  builds: `services/push/push_server` hosts `PushGrpcService + PushRuntime`
  and listens on `push.listen_address` (default `0.0.0.0:9101`). Its current
  default adapters remain transitional no-ops when no Gateway delivery endpoint
  is configured.
- Remote Push delivery plumbing first slice is implemented behind explicit
  gRPC builds: `im.push.GatewayPushDeliveryService` exposes Gateway-owned
  session lookup, session payload send, and delivered marking; Gateway remote
  mode starts this internal delivery endpoint through
  `push.gateway_delivery_listen_address`; `push_server` can use
  `push.gateway_delivery_endpoint` to replace its no-op adapters with
  remote-aware Gateway adapters.
- vcpkg root is configured for `/home/myself/pkgs/vcpkg`.

## Completed Work

- Root CMake staged options were added:
  - `MYCHAT_BUILD_TESTS`
  - `MYCHAT_BUILD_GATEWAY`
  - `MYCHAT_BUILD_SERVICES`
  - `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`
  - `MYCHAT_BUILD_PGSQL_ODB`
- Optional `codec-grpc` vcpkg feature was added for the gRPC runtime/plugin
  used by `MYCHAT_BUILD_CODEC_SERVICE=ON` and
  `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
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
- Full ODB-enabled baseline: 9/9 suites passed. No-ODB build skips Message
  Service and Friend Service targets cleanly.
- Gateway Message HTTP integration (Task 004):
  - Added `MessageHttpController` with `handle_send`, `handle_history`, and
    `handle_offline`.
  - Routes: POST `/api/v1/messages/send`, GET `/api/v1/messages/history`, GET
    `/api/v1/messages/offline`.
  - All Message HTTP routes require a valid Bearer access token; token UID is
    used as sender/actor and client-supplied sender identity is ignored.
  - Offline pull marks returned messages delivered and returns updated status.
  - Message HTTP route registration occurs before legacy catch-all handlers.
  - `odb_db_` and ODB initialization are guarded for either User HTTP or
    Message HTTP so message-only staged builds are structurally safe.
  - `GatewayMessageHttpTest` covers authenticated send, missing/invalid token,
    invalid body, token UID trust, history, missing `peer_uid`, offline pull
    auto-delivery, and route-layer registration.
  - Full ODB-enabled baseline: 7/7 suites passed. No-ODB build skips Message
    HTTP targets cleanly.
- Friend Service MVP stabilized with focused tests:
  - `FriendServiceCoreTest` — 14 service-level tests covering send request,
    empty/self reject, duplicate (pair + reverse) reject, target-only
    accept/reject, non-pending reject, not-found, accepted friends list,
    pending requests list, reject status, and empty list edge cases.
  - `GatewayFriendHttpTest` — 16 Gateway HTTP tests covering missing/invalid
    token, invalid JSON, missing fields, self-request, duplicate, forbidden,
    not-found, pending list, friends list, route registration, and happy-path
    send/respond/list flows.
  - All four Friend HTTP routes (`/api/v1/friends/request`,
    `/api/v1/friends/respond`, `/api/v1/friends`, `/api/v1/friends/pending`)
    registered before legacy catch-all with route-layer integration test.
  - Deterministic cleanup via test-prefixed UIDs; no DROP TABLE.
  - ODB-enabled baseline when Friend MVP landed: 11/11 suites passed. No-ODB:
    2/2 passed.

## In Progress

- Message Service MVP (Phase F) is nearly complete. Persistence core, Gateway
  HTTP integration, Gateway WebSocket send/ack, online delivery, PushService
  with FanoutPolicy, production fanout policies (PlatformFilter,
  NewestSession), group multi-recipient fanout, PushNotifier boundary, and
  behavior-preserving push/fanout characterization tests are complete. Fanout
  policy logic and PushRuntime live in `services/push`; the Push gRPC contract
  server-side adapter, Gateway remote PushNotifier client, standalone
  `push_server` process target, and first Gateway delivery callback channel are
  in place. The remaining service-call work is an end-to-end remote-mode smoke
  and operational hardening of the two-process startup/config path.
- Friend Service MVP (Phase G) is complete. Persistence model, repository,
  service, and Gateway HTTP controller have focused tests passing, API contract
  documented with TARGET_NOT_FOUND validation and HTTP status mapping.
- Group Service MVP (Phase H) is complete. ODB schema (group + group_member),
  GroupRepository, GroupService, GroupMessageService service-level validation
  tests (17 total cases in the group test binary), Gateway HTTP controller and
  tests (23 cases), GroupMessage ODB model, message send/history HTTP
  controller and tests (15 cases), and multi-recipient group message fanout via
  PushService are all passing.

## Next Immediate Tasks

1. Add an end-to-end local/remote Push smoke: run `push_server`, set Gateway
   `push.mode=remote`, and verify direct/group fanout still preserves
   best-effort semantics.
2. Harden remote Push startup/config behavior: document/validate
   `push.remote_endpoint`, `push.gateway_delivery_listen_address`, and
   `push.gateway_delivery_endpoint`; decide failure vs degraded local fallback
   policy for unavailable internal delivery endpoints.
3. Fix `pgsql_conn.hpp` template wrapper issues (string ID handling) when
   it becomes a blocker.

## Risks

- ODB 2.5.0 runtime must be built from source (until vcpkg packages are
  updated). Developers must run `scripts/build_odb_runtime_2_5.sh` before
  enabling ODB builds. CMake fails at configure time if runtime is missing.
- `pgsql_conn.hpp` RAII wrapper has pre-existing issues (string-ID handling,
  raw-pointer return). User Service bypasses it by using `odb::pgsql::database`
  directly; Friend and Group services follow the same direct database pattern.
  Fix this wrapper only when a future task chooses to use it.
- Inactive duplicate generated files still exist under `services/codec`, but
  no active target compiles them. Future cleanup can delete or archive them
  after verifying no legacy include path still depends on them.
- Old tests still contain references to removed dependencies and may fail if
  re-enabled wholesale.
- Current Redis wrapper is single-connection and mutex-serialized. It is enough
  for correctness tests, not for performance claims.
- Full Phase F is not complete: remote Push end-to-end smoke, remote startup
  hardening, and schema migration remain future work. PushService with
  pluggable FanoutPolicy, service-owned production fanout policies, group
  multi-recipient fanout, PushNotifier boundary/tests, PushRuntime core
  extraction, codec/gRPC generation cleanup, the Push gRPC contract/adapter,
  Gateway remote PushNotifier client wiring, the standalone `push_server`
  process target, and the first Gateway delivery callback channel are complete.
- `SendRequest::msg_type` is caller-supplied even though the method is named
  `send_text_message`; defaulting it to `MessageType::TEXT` is a future cleanup.
- `AuthTokenTest.IndependentExpiryPerRefreshToken` showed a timing-sensitive
  transient failure on one run and passed on retry; this appears pre-existing
  and unrelated to Task 004.
- Friend Service MVP tests are written and passing:
  - FriendServiceCoreTest: 14 test cases covering send request, empty/self
    reject, duplicate pair and reverse reject, target-only accept/reject,
    non-pending reject, not-found reject, friends list, pending list, reject
    status, empty list edge cases.
  - GatewayFriendHttpTest: 16 test cases covering missing/invalid token,
    invalid JSON, missing fields, self-request, duplicate, forbidden,
    not-found, pending list, friends list, route registration, and happy-path
    send/respond/list flows.
  - Friend HTTP routes (POST /api/v1/friends/request, POST
    /api/v1/friends/respond, GET /api/v1/friends, GET /api/v1/friends/pending)
    are registered before legacy catch-all and covered by route-layer
    integration test.
  - Friend Service API contract documented: send_request rejects nonexistent
    target_uid with `TARGET_NOT_FOUND` (HTTP 404). All error codes and
    success responses documented in phase12 devlog.
  - Both tests use test-prefixed UIDs for deterministic cleanup, no DROP TABLE.
  - Full ODB-enabled baseline after Group MVP: 14/14 suites passed. No-ODB
    build skips ODB-backed service targets cleanly (2/2).

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
- Gateway Message HTTP integration: `docs/devlog/phase7_gateway_message_http.md`
- Gateway WebSocket send/ack: `docs/devlog/phase8_gateway_message_ws_ack.md`
- Gateway online message delivery: `docs/devlog/phase9_gateway_online_delivery.md`
- PushService with FanoutPolicy: `docs/devlog/phase10_push_service_fanout.md`
- Fanout policies: `docs/devlog/phase11_fanout_policies.md`
- Friend Service MVP: `docs/devlog/phase12_friend_service_mvp.md`
- Group Service MVP: `docs/devlog/phase13_group_service_mvp.md`
- Agent context: `docs/agent_context/project_context.md`, `architecture_analysis.md`, `roadmap.md`, `todo.md`
- Codgent task001 final record: `docs/agent_context/tasks/task001/final.md`
- Codgent task003 final record: `docs/agent_context/tasks/task003/final.md`
- Codgent task004 final record: `docs/agent_context/tasks/task004/final.md`
