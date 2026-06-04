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
  multi-recipient group message fanout via PushService. Full baseline: 14/14 suites.

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
  Current adapters are explicit no-op/no-session transitional adapters because
  Gateway still owns live WebSocket sessions.

## Current

- [ ] Message Service MVP (Phase F) - real remote Push delivery plumbing.
  Persistence core (Task 003), Gateway HTTP
  integration (Task 004), WebSocket send/ack (Task 006), online delivery
  (Task 007), PushService with FanoutPolicy (Task 008), production fanout
  policies, multi-recipient fanout, PushNotifier boundary/tests, PushRuntime
  core extraction, Push gRPC contract/adapter, Gateway remote-client strategy,
  and standalone `push_server` target are complete.

## Next

- [ ] Design and implement the remote Push delivery channel between Gateway and
  `push_server` for session lookup, payload send, and delivered marking.
- [ ] Add an end-to-end remote Push smoke: run standalone Push server, set
  Gateway `push.mode=remote`, and verify direct/group fanout still use
  `PushNotifier` best-effort semantics.
- [ ] Fix `pgsql_conn.hpp` template wrapper string-ID handling (if it becomes a blocker for new service development).
- [ ] Add connection pool to Redis wrapper before load/performance testing.

## Blocked

- ODB 2.5.0 runtime build from source — not yet available in vcpkg; CMake fails at configure time if runtime is missing.
- Inactive duplicate codec generated files remain under `services/codec`; they
  are out of the active build path and should only be removed after a focused
  legacy include-path check.
