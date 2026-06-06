---
type: todo
status: active
updated_by: coder
---

# Todo

## Completed

- [x] Group Service MVP - ODB schema (group + group_member), GroupRepository,
  GroupService + GroupMessageService, 17 service-level tests (14 GroupServiceTest
  cases + 3 GroupMessageServiceTest cases), Gateway HTTP controller + 23
  integration tests (GatewayGroupHttpTest), GroupMessage ODB model + HTTP
  controller (send/history) + 15 tests (GatewayGroupMessageHttpTest),
  multi-recipient group message fanout via PushService. Latest full ODB +
  Gateway + Push gRPC baseline: 23/23 suites.

## Completed (Previous)

- [x] Message Service persistence core (Task 003) - ODB-backed `im_messages`,
  validated direct text send, chronological conversation history, offline pull,
  and delivered/read marking.
- [x] Gateway Message HTTP integration (Task 004) - authenticated send,
  conversation history, and offline pull routes; token UID is sender/actor;
  offline pull marks returned messages delivered.
- [x] Gateway WebSocket send/ack (Task 006) - CMD_SEND_MESSAGE handler,
  protobuf SendMessageRequest/SendMessageResponse, token-derived sender
  identity, protobuf type validation, cmd_id validation, protobuf error
  responses.
- [x] Gateway online message delivery (Task 007) - after successful
  persistence, pushes CMD_PUSH_MESSAGE + PushRequest to recipient's active
  sessions via ConnectionManager and WebSocketServer; marks message delivered
  after at least one successful push. Best-effort: offline recipients stay
  undelivered.
- [x] PushService with FanoutPolicy (Task 008) - extracted push logic from
  MessageWsHandler into dedicated PushService class with pluggable FanoutPolicy.
  Default AllSessionsFanoutPolicy preserves existing behavior. MessageWsHandler
  delegates to PushService::push_to_user. Independently testable PushService.
- [x] Production fanout policies - PlatformFilterFanoutPolicy (select sessions
  matching a set of allowed platforms) and NewestSessionFanoutPolicy (select
  the single most recently connected session). These now live in
  `services/push/fanout_policy.*`.
- [x] Friend Service MVP - Friend persistence model, repository, service,
  Gateway HTTP controller with focused tests (FriendServiceCoreTest 14 cases,
  GatewayFriendHttpTest 16 cases). API contract documented: send_request
  rejects EMPTY_UID, SELF_REQUEST, TARGET_NOT_FOUND, ALREADY_EXISTS;
  respond_to_request validates FORBIDDEN/NOT_PENDING/NOT_FOUND. All Friend
  HTTP routes registered before legacy catch-all.
- [x] Codec/gRPC generation chain cleanup (Task 009) - protobuf and codec gRPC
  generation are deterministic from canonical `common/proto` inputs.
  `generate_proto` covers active protobuf, codec gRPC, and push gRPC outputs
  when the gRPC plugin is available, and `im_codec_service` no longer compiles
  duplicated generated files from `services/codec`.
- [x] Push Service boundary - `services/push` now owns the
  `im::service::push::PushNotifier` boundary, service-owned fanout policies,
  and `im::push_service` target. Gateway WS and Group Message HTTP depend on
  that boundary; focused tests pin direct-message push delegation and group
  fanout recipients.
- [x] PushRuntime core extraction - `services/push/PushRuntime` now owns fanout
  selection, protobuf push payload construction, best-effort send
  orchestration, and delivered marking decisions. Gateway `PushService` is a
  narrow adapter for `ConnectionManager`, `WebSocketServer`, and Message
  Service delivered marking.
- [x] Gateway Push/WS build gating cleanup - Message HTTP remains gated only on
  Message Service; `PushService`, `MessageWsHandler`, and Group Message HTTP
  fanout now require `im::push_service`. A `MYCHAT_BUILD_SERVICES=OFF`
  Gateway-only configure/build passed.
- [x] Push gRPC remote boundary - `common/proto/push.proto` now defines
  `im.push.PushService.NotifyUser`; `generate_push_grpc` and aggregate
  `generate_proto` produce `common/proto/push.grpc.pb.*`; `im::push_grpc_service`
  adapts generated gRPC calls to the existing `PushNotifier` boundary behind
  `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
- [x] Gateway remote PushNotifier wiring - `gateway/push/RemotePushNotifier`
  wraps the generated `im.push.PushService::Stub`; `GatewayServer` selects
  local vs. remote through `push.mode`; `config/dev.json` defaults to
  `push.mode = "local"` so the existing in-process adapter remains the default.
- [x] Standalone Push server process slice - `services/push/push_server` hosts
  `PushGrpcService + PushRuntime` behind `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
  It keeps no-op adapters only when no Gateway delivery endpoint is configured.
- [x] Remote Push delivery callback channel first slice - `push.proto` now
  defines `GatewayPushDeliveryService`; Gateway remote mode exposes session
  lookup, payload send, and delivered marking on
  `push.gateway_delivery_listen_address`; `push_server` can call back through
  `push.gateway_delivery_endpoint` via remote-aware adapters.
- [x] Remote Push gRPC-link E2E smoke - `RemotePushEndToEndSmokeTest` starts a
  real Gateway delivery gRPC endpoint plus a real `PushServerApp`, calls
  `NotifyUser`, and verifies session lookup, payload send, protobuf payload
  contents, delivered marking, and offline/no-session best-effort behavior.
- [x] Remote Push Gateway entrypoint smoke - `RemotePushGatewayEntrypointsTest`
  injects a real `RemotePushNotifier` into `MessageWsHandler` and
  `GroupMessageHttpController`, then verifies direct WS and group HTTP send
  paths reach `push_server` and Gateway delivery callbacks with preserved
  direct/group fanout semantics.
- [x] Full GatewayServer process-level remote Push smoke -
  `RemotePushGatewayServerSmokeTest` starts a real `PushServerApp`, starts a
  real `GatewayServer` with `push.mode=remote`, verifies HTTP health on the
  real HTTP port, connects sender/receiver TLS WebSocket clients on the real
  WS port, sends `CMD_SEND_MESSAGE`, receives the sender ack, and verifies the
  receiver gets `CMD_PUSH_MESSAGE` through the remote Push callback path.
- [x] Push server invalid-listen startup guard - `PushServerAppTest` verifies an
  invalid `push.listen_address` returns false and does not mark the server
  running.
- [x] Remote Push local two-process config seed -
  `config/dev.remote-push.json` configures Gateway remote mode and requires
  `push_server` to use Gateway delivery callbacks instead of silently falling
  back to no-session/no-op adapters.
- [x] Push server required callback endpoint guard - `PushServerAppTest`
  verifies `push_server` startup fails when
  `require_gateway_delivery_endpoint=true` and the endpoint is blank.
- [x] PostgreSQL schema migration baseline -
  `db/migrations/001_core_schema.sql` defines the current core IM schema
  non-destructively, and `scripts/db/migrate_postgres.sh` applies ordered SQL
  migrations with version/checksum tracking in `schema_migrations`.
- [x] Service-level schema setup consolidation -
  User, Message, Friend, and Group service tests now use
  `test/support/postgres_schema.*` instead of duplicating table creation SQL.
- [x] Gateway-level schema setup consolidation -
  Focused Gateway User, Message HTTP/WS, PushService, Friend, Group, and Group
  Message tests now use `test/support/postgres_schema.*` instead of duplicating
  table creation SQL.
- [x] Remote Push smoke schema setup consolidation -
  Remote Push Gateway entrypoint and full GatewayServer smoke fixtures now use
  `test/support/postgres_schema.*`.

## Current

- [ ] Message/Push runtime hardening (Phase F closeout) - finish remote Push
  startup/config behavior and continue adopting the persistence migration
  baseline in local runtime setup.
  Persistence core (Task 003), Gateway HTTP
  integration (Task 004), WebSocket send/ack (Task 006), online delivery
  (Task 007), PushService with FanoutPolicy (Task 008), production fanout
  policies, multi-recipient fanout, PushNotifier boundary/tests, PushRuntime
  core extraction, Push gRPC contract/adapter, Gateway remote-client strategy,
  standalone `push_server` target, first Gateway delivery callback channel,
  gRPC-link remote Push smoke, and Gateway handler/controller entrypoint remote
  Push smoke, and full GatewayServer remote Push smoke are complete.

## Next

- [ ] Decide the local runtime migration policy: explicit
  `scripts/db/migrate_postgres.sh` before startup versus dev-only service
  startup integration.
- [ ] Decide whether the isolated low-level ODB persistence test should keep
  its narrow single-table setup or move to `test/support/postgres_schema.*`.
- [ ] Extend remote Push real-server coverage only where it adds new signal.
- [ ] Fix `pgsql_conn.hpp` template wrapper string-ID handling (if it becomes a blocker for new service development).
- [ ] Add connection pool to Redis wrapper before load/performance testing.

## Blocked

- ODB 2.5.0 runtime build from source — not yet available in vcpkg; CMake fails at configure time if runtime is missing.
- Inactive duplicate codec generated files remain under `services/codec`; they
  are out of the active build path and should only be removed after a focused
  legacy include-path check.
