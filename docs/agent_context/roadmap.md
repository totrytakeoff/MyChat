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
         delivery + fanout policies + group multi-recipient fanout + PushNotifier
         boundary/tests + PushRuntime core extraction + codec/gRPC generation
         cleanup + Push gRPC contract/adapter + Gateway remote-client wiring
         + standalone Push server process + first Gateway delivery callback
         channel + gRPC-link remote E2E smoke + Gateway handler/controller
         entrypoint remote smoke + full gateway_server process remote smoke
         complete; deeper startup/config hardening pending).

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
  session). These now live in `services/push/fanout_policy.*`.
- [x] Multi-recipient fanout for group messages via `PushService::push_to_user`
  per group member.
- [x] Push Service boundary: `services/push` provides
  `im::service::push::PushNotifier`, service-owned fanout policies, and
  `im::push_service`; Gateway direct and group message call sites now depend on
  the boundary instead of concrete `PushService`.
- [x] PushRuntime core extraction: `services/push/PushRuntime` owns fanout,
  payload encoding, send orchestration, and delivered marking decisions;
  Gateway `PushService` is now an adapter for session lookup, payload send, and
  delivered marking.
- [x] Gateway Push/WS build gating cleanup: Message HTTP remains independently
  gated on Message Service; `PushService`, `MessageWsHandler`, and Group
  Message HTTP fanout now require `im::push_service` so disabled service
  modules do not leave Gateway half-enabled.
- [x] Push behavior characterization tests: WebSocket direct-message send
  notifies the receiver through `PushNotifier`; group-message send notifies
  other members only and excludes the sender.
- [x] Codec/gRPC generation chain cleanup from canonical `common/proto` inputs:
  `generate_common_proto`, `generate_codec_grpc`, and aggregate
  `generate_proto` now regenerate active protobuf/gRPC files; `im_codec_service`
  consumes canonical generated files from `common/proto`.
- [x] Push gRPC remote boundary: `common/proto/push.proto` defines
  `im.push.PushService.NotifyUser`; `generate_push_grpc` and aggregate
  `generate_proto` produce `common/proto/push.grpc.pb.*`; `im::push_grpc_service`
  adapts the generated service to `PushNotifier` behind
  `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
- [x] Gateway remote PushNotifier wiring: `gateway/push/RemotePushNotifier`
  wraps the generated `im.push.PushService::Stub`; `GatewayServer` selects the
  local in-process adapter vs. remote gRPC client through `push.mode`; default
  config remains `local`.
- [x] Standalone Push server process: `services/push/push_server` hosts
  `PushGrpcService + PushRuntime` and reads `push.listen_address` from config.
  It keeps no-session/no-op adapters only when no Gateway delivery endpoint is
  configured.
- [x] Remote Push delivery callback channel first slice:
  `GatewayPushDeliveryService` exposes Gateway-owned session lookup, payload
  send, and delivered marking; Gateway remote mode starts it on
  `push.gateway_delivery_listen_address`; `push_server` can call it through
  `push.gateway_delivery_endpoint` with remote-aware adapters.
- [x] Remote Push gRPC-link E2E smoke:
  `RemotePushEndToEndSmokeTest` starts a real Gateway delivery endpoint plus a
  real `PushServerApp`, calls `NotifyUser`, and verifies session lookup,
  payload send, protobuf payload contents, delivered marking, and offline
  no-session best-effort behavior across the remote callback chain.
- [x] Remote Push Gateway entrypoint smoke:
  `RemotePushGatewayEntrypointsTest` injects a real `RemotePushNotifier` into
  `MessageWsHandler` and `GroupMessageHttpController`, then verifies direct WS
  and group HTTP send paths reach `push_server` and Gateway delivery callbacks
  with preserved direct-message delivery and group fanout-to-members-only
  semantics.
- [x] Full GatewayServer process-level remote Push smoke:
  `RemotePushGatewayServerSmokeTest` starts a real `PushServerApp` and a real
  `GatewayServer` configured with `push.mode=remote`, verifies HTTP health on
  the real HTTP port, connects sender/receiver TLS WebSocket clients on the
  real WS port, sends `CMD_SEND_MESSAGE`, receives the sender ack, and verifies
  the receiver gets `CMD_PUSH_MESSAGE` through the remote Push callback path.
- [x] Push server startup guard: invalid `push.listen_address` returns false
  and does not mark the server running.
- Remaining exit criteria: Message Service persistence tests pass; Gateway HTTP
  message API passes; Gateway can deliver messages to online users; offline
  messages are persisted and pullable. Remote Push now has a callback channel,
  real gRPC-link smoke, Gateway handler/controller entrypoint smoke, and full
  `GatewayServer` process-level WS smoke; deeper endpoint startup/config
  hardening remains.

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
- ✅ Full ODB + Gateway + Push gRPC baseline: 23/23 test suites passing.
