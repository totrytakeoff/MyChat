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
  regenerates `common/proto/push.grpc.pb.*`, `generate_user_grpc`
  regenerates `common/proto/user.grpc.pb.*`, and aggregate `generate_proto`
  covers active protobuf/gRPC generation when the gRPC plugin is available.
  `im::proto` now includes `codec_service.pb.cc`; `im_codec_service` consumes
  canonical generated files from `common/proto` and no longer compiles
  duplicated `services/codec/base.pb.cc`. The inactive duplicated
  `services/codec/*.pb.*` files have been removed; `services/codec` only keeps
  the placeholder codec service source.
- Push gRPC remote boundary introduced: `common/proto/push.proto` now defines
  `im.push.PushService.NotifyUser`, `NotifyUserRequest`, and
  `NotifyUserResponse`; `common/proto/push.pb.*` and
  `common/proto/push.grpc.pb.*` were regenerated through CMake
  `generate_proto`. `services/push/PushGrpcService` adapts the generated gRPC
  service to the existing `PushNotifier` boundary. `im::push_grpc_service` is
  gated by `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`, so default builds still do not
  require gRPC.
- User gRPC remote boundary first slice is implemented:
  `common/proto/user.proto` now defines `im.user.UserService` with
  `Register`, `Login`, and `GetUserInfo`; `common/proto/user.pb.*` and
  `common/proto/user.grpc.pb.*` were regenerated through CMake
  `generate_proto`. `services/user/UserGrpcService` adapts generated gRPC
  calls to the existing ODB-backed `UserService` semantics. The target
  `im::user_grpc_service` is gated by `MYCHAT_BUILD_USER_GRPC_SERVICE=ON`, so
  default builds still do not require gRPC.
- Message gRPC remote boundary first slice is implemented:
  `common/proto/message.proto` now defines `im.message.MessageService` with
  `SendMessage`, `GetConversation`, `PullOffline`, `MarkDelivered`, and
  `MarkRead`; `common/proto/message.pb.*` and
  `common/proto/message.grpc.pb.*` were regenerated through CMake
  `generate_proto`. `services/message/MessageGrpcService` adapts generated
  gRPC calls to the existing ODB-backed `MessageService` semantics. The target
  `im::message_grpc_service` is gated by `MYCHAT_BUILD_MESSAGE_GRPC_SERVICE=ON`,
  so default builds still do not require gRPC.
- Standalone Message server process slice is implemented behind explicit gRPC
  builds: `services/message/message_server` hosts `MessageGrpcService` around
  the ODB-backed `MessageService`, reads PostgreSQL settings from `postgres.*`
  via `message_server_main`, and listens on `message.listen_address` (default
  `0.0.0.0:9002`). `MessageServerAppTest` starts the server on an ephemeral
  port and verifies send, offline pull, and delivered marking through a
  generated gRPC stub against Docker PostgreSQL.
- Standalone User server process slice is implemented behind explicit gRPC
  builds: `services/user/user_server` hosts `UserGrpcService` around the
  ODB-backed `UserService`, reads PostgreSQL settings from `postgres.*` via
  `user_server_main`, and listens on `user.listen_address` (default
  `0.0.0.0:9001`). `UserServerAppTest` starts the server on an ephemeral port
  and verifies `Register`, `Login`, and `GetUserInfo` through a generated gRPC
  stub against Docker PostgreSQL.
- Gateway remote User client wiring is implemented behind explicit gRPC builds:
  `gateway/http/UserClient` is the local/remote facade consumed by
  `UserHttpController`; `LocalUserClient` wraps the existing in-process
  `UserService`, and `RemoteUserClient` wraps the generated
  `im.user.UserService::Stub`. `GatewayServer` selects local vs. remote through
  `user.mode`, with `config/dev.json` defaulting to `user.mode = "local"` so
  existing HTTP behavior remains the default. `RemoteUserGatewayServerSmokeTest`
  starts a real `UserServerApp` plus a real `GatewayServer` in remote User mode
  and verifies `/api/v1/auth/register`, `/api/v1/auth/login`, and
  `/api/v1/auth/info` preserve the current external HTTP contract.
- Gateway remote Message client wiring is implemented behind explicit gRPC
  builds: `gateway/http/MessageClient` is the local/remote facade consumed by
  Message HTTP, Message WS, and Push delivered marking; `LocalMessageClient`
  wraps the existing in-process `MessageService`, and `RemoteMessageClient`
  wraps the generated `im.message.MessageService::Stub`. `GatewayServer`
  selects local vs. remote through `message.mode`, with `config/dev.json`
  defaulting to `message.mode = "local"` so existing HTTP/WS behavior remains
  the default. `RemoteMessageGatewayServerSmokeTest` starts a real
  `MessageServerApp` plus a real `GatewayServer` in remote Message mode and
  verifies `/api/v1/messages/send`, `/api/v1/messages/history`, and
  `/api/v1/messages/offline` preserve the current external HTTP contract while
  using the remote Message process.
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
- Remote Push gRPC-link smoke is implemented: `RemotePushEndToEndSmokeTest`
  starts a real `GatewayPushDeliveryService` endpoint and a real
  `PushServerApp`, then calls `PushService.NotifyUser` through a generated
  stub to verify session lookup, payload send, protobuf payload contents, and
  delivered marking across the remote callback chain.
- Remote Push Gateway entrypoint smoke is implemented:
  `RemotePushGatewayEntrypointsTest` injects a real `RemotePushNotifier` into
  `MessageWsHandler` and `GroupMessageHttpController`, then verifies direct WS
  send and group HTTP send reach `push_server` and Gateway delivery callbacks
  with preserved direct-message delivery and group fanout-to-members-only
  semantics.
- Remote Push full Gateway-server smoke is implemented:
  `RemotePushGatewayServerSmokeTest` starts a real `PushServerApp`, starts a
  real `GatewayServer` with `push.mode=remote`, connects sender/receiver
  WebSocket clients through the real TLS WS port, verifies HTTP health through
  the real HTTP port, sends `CMD_SEND_MESSAGE`, receives the sender ack, and
  verifies the receiver gets `CMD_PUSH_MESSAGE` through the remote
  `push_server -> GatewayPushDeliveryService -> Gateway-owned session` path.
- Gateway remote Push startup/config hardening first slice is implemented:
  `push.mode=remote` now requires explicit non-blank `push.remote_endpoint`
  and `push.gateway_delivery_listen_address`, and a blank Gateway delivery
  listen address fails startup instead of silently disabling the callback
  endpoint. `RemotePushGatewayServerSmokeTest` covers both config-failure
  cases plus the real WS remote Push smoke.
- Configured-but-unavailable remote `push_server` behavior is pinned:
  Gateway still starts when `push.remote_endpoint` is syntactically configured
  but no remote Push server is listening, WS send keeps the existing
  best-effort success ack, and the message remains `SENT` and pullable through
  the offline path instead of being incorrectly marked delivered.
- Real Gateway HTTP group-message remote Push coverage is implemented:
  `RemotePushGatewayServerSmokeTest` now posts to
  `/api/v1/groups/messages/send` on a real Gateway HTTP port, with real
  `GatewayServer`, real `PushServerApp`, and online WebSocket group members,
  then verifies members receive `CMD_PUSH_MESSAGE` through the remote
  callback path.
- Remote Push endpoint topology is documented in `gateway/README.md` and this
  phase devlog: `push.listen_address` belongs to `push_server`,
  `push.remote_endpoint` is Gateway's client target for Push,
  `push.gateway_delivery_listen_address` is Gateway's callback listen address,
  and `push.gateway_delivery_endpoint` is Push server's client target for
  Gateway callbacks.
- Remote Push local two-process development seed is in place:
  `config/dev.remote-push.json` configures Gateway remote mode and requires
  `push_server` to have a Gateway callback endpoint, preventing the real remote
  topology from silently falling back to no-session/no-op adapters.
- PostgreSQL schema migration baseline is in place:
  `db/migrations/001_core_schema.sql` defines the current core IM tables
  non-destructively, and `scripts/db/migrate_postgres.sh` applies ordered SQL
  migrations while recording version/checksum state in `schema_migrations`.
- Service-level and focused Gateway-level persistence tests now share
  `test/support/postgres_schema.*` for core table setup instead of duplicating
  local `CREATE TABLE IF NOT EXISTS` blocks.
- Remote Push Gateway entrypoint and full GatewayServer smoke fixtures also
  share `test/support/postgres_schema.*`; the low-level ODB user persistence
  test has also moved to the shared helper.
- Local runtime migration policy is explicit:
  `scripts/dev/prepare_runtime.sh` starts Redis/PostgreSQL when available and
  runs migrations before service startup; service binaries do not run
  migrations implicitly. `scripts/dev/run_remote_push_stack.sh` wraps the
  local remote Push two-process startup.
- CI/CD engineering baseline local scripts are in place:
  `scripts/ci/checks.sh`, `scripts/ci/default_regression.sh`, and
  `scripts/ci/remote_push_odb.sh` provide local reusable CI entrypoints.
  GitHub hosted CI is paused during feature development; revisit it near
  stabilization/release hardening.
- vcpkg root is configured for `/home/myself/pkgs/vcpkg`.

## MVP Review State (2026-06-09)

The project is now treated as a complete practical MVP for review, manual
testing, and interview preparation. The next workstream should not keep
expanding the feature surface as if every production IM feature is blocking.
Instead, it should make the existing system easy to run, verify, explain, and
demonstrate as a distributed IM backend plus validation Web client.

Current user-facing MVP capabilities:

- Account registration, login, token-backed profile lookup, and profile update.
- Editable profile fields include nickname, gender, signature, and avatar.
  Avatar is currently stored in the existing `avatar` text field and accepts
  either a URL or a browser-generated base64 data URL. A dedicated upload/file
  storage service is a future extension, not an MVP blocker.
- Direct one-to-one message send, history query, offline pull, WebSocket
  send/ack, and online push.
- Friend search by account/nickname, friend request, pending request view,
  respond/accept/reject, and friend list.
- Group creation, search, info view, join/leave, member list, group message
  send/history, and online group fanout.
- A Chinese Web validation client under `clients/web` that follows a normal IM
  workflow: auth screen, left navigation, conversation/contact/group lists,
  centered profile/group detail views, unread badges, configurable send
  shortcut, profile editing, and a developer/debug drawer kept out of the main
  user flow.

Current distributed architecture state:

- Gateway exposes the external HTTP and WebSocket contracts.
- User, Message, Friend, Group, and Push have local service logic, gRPC service
  adapters, standalone server processes, and Gateway local/remote facades.
- `common/proto` is the canonical protobuf/gRPC source tree, and
  `generate_proto` is the aggregate generation target.
- Redis is used for auth/session-related state through a hiredis RAII
  connection pool.
- PostgreSQL plus ODB is used for durable user/message/friend/group data.
- Local all-remote runtime is available through
  `scripts/dev/run_remote_services_stack.sh`; one-command full local startup
  for backend plus Web testing is available through
  `scripts/dev/run_full_stack.sh`.

Recent validation baseline:

- Focused backend and remote-service smoke tests have covered the Gateway
  local/remote facade behavior, remote Push callback topology, WebSocket direct
  push, and group HTTP fanout.
- Web runtime validation covered two browser contexts, register/login,
  account search before friend request, friend request/accept, direct realtime
  push without manual refresh, group create/search/join, group message history,
  group realtime push, profile editing, and profile/group/contact detail
  layout corrections.
- For documentation-only changes, `git diff --check` is sufficient. Re-run
  backend/Web builds only when implementation files change.

Resume/interview framing:

- The project should be presented as a C++ distributed IM backend with Gateway,
  service boundaries, gRPC-based internal RPC, Redis-backed auth/session
  state, PostgreSQL/ODB persistence, WebSocket realtime delivery, and a Web
  validation client.
- The strongest talking points are not "a chat UI exists"; they are the
  service-boundary migration, Gateway local/remote facade strategy, protobuf
  contract ownership, Push fanout/runtime split, remote callback path for
  Gateway-owned WebSocket sessions, schema migration baseline, and the ability
  to validate flows through both automated tests and an actual Web client.
- Production-grade items such as rich media upload, blacklist/privacy,
  message recall/edit/delete, group administration, hosted CI, deployment
  packaging, production TLS/secret management, and load benchmarking should be
  listed as planned extensions unless a later phase explicitly prioritizes
  them.

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
  - `PgSqlConnectionTest` now covers the shared `pgsql_conn` wrapper for
    string UID persist/load/find/update/erase and void transaction lambdas.
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
  `push_server` process target, first Gateway delivery callback channel, full
  `GatewayServer` remote-mode WS smoke, Gateway remote-mode required endpoint
  validation, unavailable remote-endpoint best-effort semantics, and endpoint
  topology documentation are in place. Real Gateway HTTP group-message send is
  now covered through the remote Push path. The Message gRPC boundary and
  standalone `message_server` process are also in place; Gateway remote Message
  client wiring is complete for Message HTTP, Message WS persistence, and Push
  delivered marking through `message.mode=remote`. Hosted CI is intentionally
  paused; local regression scripts remain the active verification path.
- Friend Service MVP (Phase G) is complete. Persistence model, repository,
  service, and Gateway HTTP controller have focused tests passing, API contract
  documented with TARGET_NOT_FOUND validation and HTTP status mapping.
- Friend gRPC boundary first slice is complete behind explicit gRPC builds:
  `common/proto/friend.proto` now defines `im.friend_.FriendService` with
  `SendRequest`, `RespondToRequest`, `GetFriends`, and
  `GetPendingRequests`; `common/proto/friend.pb.*` and
  `common/proto/friend.grpc.pb.*` were regenerated through CMake
  `generate_proto`. `services/friend/FriendGrpcService` adapts generated gRPC
  calls to the existing ODB-backed `FriendService` semantics, and
  `services/friend/friend_server` hosts that adapter as a standalone process
  on `friend.listen_address` (default `0.0.0.0:9003`). The gRPC package uses
  `im.friend_` because `friend` is a C++ keyword and the gRPC plugin does not
  escape it safely.
- Gateway remote Friend client wiring is complete behind explicit gRPC builds:
  `gateway/http/FriendClient` is the local/remote facade consumed by
  `FriendHttpController`; `LocalFriendClient` preserves the existing
  in-process `FriendService` path, and `RemoteFriendClient` wraps the
  generated `im.friend_.FriendService::Stub`. `GatewayServer` selects local
  vs. remote through `friend.mode`, with `config/dev.json` defaulting to
  `friend.mode = "local"`. `RemoteFriendGatewayServerSmokeTest` starts a real
  `FriendServerApp` plus a real `GatewayServer` in remote Friend mode and
  verifies request, pending, respond, and friends HTTP routes preserve the
  current external contract through the remote Friend process.
- Group Service MVP (Phase H) is complete. ODB schema (group + group_member),
  GroupRepository, GroupService, GroupMessageService service-level validation
  tests (17 total cases in the group test binary), Gateway HTTP controller and
  tests (23 cases), GroupMessage ODB model, message send/history HTTP
  controller and tests (15 cases), and multi-recipient group message fanout via
  PushService are all passing.
- Group gRPC boundary and standalone server slice is complete behind explicit
  gRPC builds: `common/proto/group.proto` now defines
  `im.group.GroupService` for create/join/leave/existence/list, member listing,
  group message send, and group message history; `common/proto/group.pb.*` and
  `common/proto/group.grpc.pb.*` were regenerated through CMake
  `generate_proto`. `services/group/GroupGrpcService` adapts generated gRPC
  calls to the existing ODB-backed `GroupService` and `GroupMessageService`
  semantics, and `services/group/group_server` hosts that adapter as a
  standalone process on `group.listen_address` (default `0.0.0.0:9004`).
- Gateway remote Group facade slice is complete behind explicit gRPC builds.
  `gateway/http/GroupClient` now fronts both Group HTTP and Group Message HTTP
  controllers; `LocalGroupClient` preserves the current local service path and
  `RemoteGroupClient` wraps generated `im.group.GroupService::Stub`.
  `GatewayServer` selects the facade through `group.mode`, `group.remote_endpoint`,
  and `group.timeout_ms`, defaulting to local. Focused coverage now includes
  `RemoteGroupClientTest`, `RemoteGroupGatewayServerSmokeTest`, direct
  `GroupExists` gRPC coverage, and the existing Gateway Group/Group Message
  HTTP tests. The process smoke starts a real `GroupServerApp`, starts a real
  `GatewayServer` with `group.mode=remote`, and verifies create/join/list/
  members/send/history over HTTP.
- Phase 18 stabilization first pass is green locally. `scripts/ci/checks.sh`
  passed; `scripts/ci/default_regression.sh` passed 2/2 tests; the heavy
  remote-services ODB/gRPC build passed after updating
  `RemotePushGatewayEntrypointsTest` to use `LocalMessageClient` and
  `LocalGroupClient` facades. Focused Push tests passed 10/10 and the full
  `build/remote-push-odb` CTest suite passed 40/40.
- Full local remote runtime assets are present:
  `config/dev.remote-all.json` sets User, Message, Friend, Group, and Push to
  remote mode; `scripts/dev/run_remote_services_stack.sh` starts all service
  servers plus Gateway; `docs/history/devlog/phase18_remote_runtime_runbook.md`
  documents startup order, endpoints, health smoke, and troubleshooting.
- Serial ODB test policy is now encoded in `scripts/ci/remote_push_odb.sh`.
  Focused remote Gateway smokes passed 6/6 with `--parallel 1`, and default
  no-ODB/no-gRPC regression passed 2/2 after the policy update.

## Next Immediate Tasks

1. Keep service repositories on the direct `odb::pgsql::database` pattern
   unless a new slice explicitly chooses to adopt `PgSqlConnection`.
2. Keep ODB-backed full-suite regression serial. `scripts/ci/remote_push_odb.sh`
   enforces `--parallel 1`; do not remove that until test data isolation is
   strengthened enough for parallel CTest.
3. Revisit hosted CI only after local heavy regression and runtime startup stay
   boring for several iterations.

## Risks

- ODB 2.5.0 runtime must be built from source (until vcpkg packages are
  updated). Developers must run `scripts/build_odb_runtime_2_5.sh` before
  enabling ODB builds. CMake fails at configure time if runtime is missing.
- `PgSqlConnection` is repaired for current string-ID ODB usage and has focused
  `PgSqlConnectionTest` coverage. User, Message, Friend, and Group services
  still use direct `odb::pgsql::database`; do not migrate them broadly without
  a separate repository-boundary plan.
- Inactive duplicate generated codec files were removed from `services/codec`;
  active codec generation remains under `common/proto`.
- Old tests still contain references to removed dependencies and may fail if
  re-enabled wholesale.
- RedisManager now uses a RAII hiredis connection pool keyed by
  `RedisConfig::pool_size`; connection wait timeout is configurable through
  `RedisConfig::pool_wait_timeout` / `redis.pool_wait_timeout` and covered by
  a pool-exhaustion test. Focused tests cover pool stats, concurrent
  borrowers, reinitialize boundaries, and an Auth token lifecycle concurrency
  slice. `PushServiceTest` also covers concurrent `ConnectionManager` session
  metadata reads and PushService session lookup without live WebSocket
  sessions, preserving best-effort undelivered behavior.
  `RemotePushGatewayServerSmokeTest` now fixes Redis pool settings and asserts
  the pool returns to idle after real TLS WebSocket registration, direct remote
  Push delivery, real HTTP group-message remote Push fanout, and a concurrent
  direct-send slice with 6 sender/receiver pairs sharing a pool of 4 Redis
  connections. RedisClient reconnects and retries once when hiredis reports a
  connection-level empty reply; `RedisHiredisTest` verifies this by killing a
  live Redis client connection with `CLIENT KILL ID` and successfully issuing
  the next command. More realistic pool sizing still needs a dedicated
  load/benchmark harness if the project wants quantified defaults.
- Phase F user-facing MVP behavior is complete. Remaining Phase F-adjacent
  hardening is operational: hosted CI reintroduction, production packaging, and
  stronger test/data isolation. PushService with pluggable FanoutPolicy,
  service-owned production fanout policies, group multi-recipient fanout,
  PushNotifier boundary/tests, PushRuntime core extraction, codec/gRPC
  generation cleanup, the Push gRPC contract/adapter, Gateway remote
  PushNotifier client wiring, standalone `push_server`, Gateway delivery
  callback channel, real gRPC-link remote Push smoke, Gateway handler/controller
  entrypoint remote Push smoke, and full `GatewayServer` WS remote Push smoke
  are complete.
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

- Docs index: `docs/README.md`
- Final summary docs: `docs/final_sum_docs/`
- Architecture: `docs/project/architecture/mvp_architecture.md`
- Roadmap: `docs/project/roadmap/service_mvp_roadmap.md`
- Current progress: `docs/project/roadmap/current_progress.md`
- Runtime entry: `docs/project/runtime/README.md`
- Interview/review plan:
  `docs/project/architecture/interview_review_plan.md`
- Historical devlogs: `docs/history/devlog/`
- Historical codgent records: `docs/history/codgent/`
- Historical agent context: `docs/history/agent_context/`
