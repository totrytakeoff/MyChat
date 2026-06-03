---
type: roadmap
status: draft
updated_by: coder
---

# Roadmap

Aligned with `docs/architecture/service_mvp_roadmap.md`. Each phase builds a
testable MVP slice of one service before advancing to the next.

## Phase A: Foundation Baseline

Status: complete.

- Root CMake staging, vcpkg manifest, Docker Compose for Redis/PostgreSQL.
- Gateway startup from config, health endpoint, hiredis Redis wrapper migration.
- Focused Redis and Auth tests.

## Phase B: Gateway Service MVP Hardening

Status: complete.

- Removed duplicate gateway implementation.
- Hardened refresh-token and access-token revocation to per-token/per-JTI Redis keys.
- Auth token tests cover per-token storage, independent TTLs, wrong-device isolation.

## Phase C: PostgreSQL/ODB Foundation

Status: complete.

- ODB 2.5.0 compiler verified at `/usr/bin/odb`.
- `scripts/build_odb_runtime_2_5.sh` for reproducible runtime build from source.
- `im_user_odb` target, ODB-generated mapping files from `services/odb/user.hpp`.
- `ODBUserPersistenceTest` passes against Docker PostgreSQL.

## Phase D: User Service MVP

Status: complete.

- `im_user_service` with `PasswordHasher` (PBKDF2-HMAC-SHA256), `UserRepository` (ODB CRUD), `UserService` (register/login/profile).
- UUID v4 via `RAND_bytes`, deterministic test cleanup, no `DROP TABLE`.
- 5 focused test cases, all passing.

## Phase E: Gateway + User Service Integration

Status: complete.

- `UserHttpController` with `handle_register`, `handle_login`, `handle_profile`.
- Route registration seam (`register_user_http_routes_on_server`) before legacy catch-all handlers.
- 10 integration tests (9 controller + 1 HTTP route-layer).

## Phase F: Message Service MVP

Status: in progress (persistence core + HTTP integration + WebSocket send/ack + online
         delivery + fanout policies + group multi-recipient fanout complete;
         standalone Push microservice and service-call strategy pending).

- ✅ Persistence core (task003): `services/message` target, ODB-backed message
  persistence, send one-to-one text, offline message pull, conversation history
  with `ORDER BY create_time`.
- ✅ Gateway HTTP integration (task004): authenticated HTTP endpoints for send,
  conversation history, and offline pull; token-based sender identity; auto-mark
  delivered on offline pull.
- ✅ Gateway WebSocket send/ack (task006): CMD_SEND_MESSAGE handler, protobuf
  SendMessageRequest/SendMessageResponse, token-derived sender identity,
  protobuf type validation, cmd_id validation, protobuf error responses.
- [x] WebSocket online delivery via ConnectionManager (task007): after successful
  persistence, pushes CMD_PUSH_MESSAGE + PushRequest to recipient's active
  sessions; marks message delivered after at least one successful push.
  Best-effort: offline recipients stay undelivered for later offline-pull.
- [x] PushService with FanoutPolicy (task008): extracted push logic from
  MessageWsHandler into dedicated PushService class with pluggable
  FanoutPolicy. Default AllSessionsFanoutPolicy preserves existing behavior.
  MessageWsHandler delegates to PushService::push_to_user.
- [x] Production fanout policies: PlatformFilterFanoutPolicy (platform-based
  session filtering) and NewestSessionFanoutPolicy (select single newest
  session). PushServiceTest expanded from 4 to 11 test cases.
- [x] Multi-recipient fanout for group messages via `PushService::push_to_user`
  per group member.
- Remaining exit criteria: Message Service persistence tests pass; Gateway HTTP message
  API passes; Gateway can deliver message to online user; offline message is
  persisted and pullable. Standalone Push service boundary and service-call
  strategy are still open.

## Phase G: Friend Service MVP

Status: complete.

- ✅ Friend Service: friend requests, accept/reject, contact list, pending list.
- ✅ Persistence model (im_friend_odb), repository, FriendService, Gateway HTTP
  controller compiled and tested.
- ✅ API contract: send_request rejects EMPTY_UID, SELF_REQUEST,
  TARGET_NOT_FOUND (404), ALREADY_EXISTS (409). respond_to_request validates
  FORBIDDEN (403), NOT_PENDING (400), NOT_FOUND (404).
- ✅ FriendServiceCoreTest: 14 test cases.
- ✅ GatewayFriendHttpTest: 16 test cases (including route-layer integration).
- ✅ Deterministic cleanup with test-prefixed UIDs; no DROP TABLE.

## Phase H: Group Service MVP

Status: complete.

- ✅ ODB schema for group metadata and group membership (`services/odb/group.hpp`).
- ✅ GroupRepository — ODB-backed CRUD for groups and members.
- ✅ GroupService — create group, join/leave, list members.
- ✅ Service-level tests (GroupServiceCoreTest + GroupMessageServiceTest: 17 test cases).
- ✅ Gateway HTTP controller (create group, join, leave, list groups, list members).
- ✅ Gateway HTTP integration tests (GatewayGroupHttpTest: 23 test cases).
- ✅ GroupMessage ODB model, GroupMessageService for group message persistence.
- ✅ GroupMessage HTTP controller (send, history) with tests (15 test cases).
- ✅ Multi-recipient group message fanout via PushService::push_to_user per member.
- ✅ Full ODB-enabled baseline: 14/14 test suites passing.
