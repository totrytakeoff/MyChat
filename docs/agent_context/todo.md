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
  the single most recently connected session). PushServiceTest expanded from
  4 to 11 test cases. ODB 9/9, no-ODB 2/2.
- [x] Friend Service MVP - Friend persistence model, repository, service,
  Gateway HTTP controller with focused tests (FriendServiceCoreTest 14 cases,
  GatewayFriendHttpTest 16 cases). API contract documented: send_request
  rejects EMPTY_UID, SELF_REQUEST, TARGET_NOT_FOUND, ALREADY_EXISTS;
  respond_to_request validates FORBIDDEN/NOT_PENDING/NOT_FOUND. All Friend
  HTTP routes registered before legacy catch-all.

## Current

- [ ] Codec/gRPC generation chain cleanup (Task 009) - make protobuf and codec
  gRPC generation deterministic from canonical `common/proto` inputs before
  standalone Push Service work.
- [ ] Message Service MVP (Phase F) - Push Service as standalone microservice
  and service-call strategy. Persistence core (Task 003), Gateway HTTP
  integration (Task 004), WebSocket send/ack (Task 006), online delivery
  (Task 007), PushService with FanoutPolicy (Task 008), production fanout
  policies, and multi-recipient fanout are complete.

## Next

- [ ] Define the standalone Push Service contract and migration sequence after
  Task 009 is complete.
- [ ] Fix `pgsql_conn.hpp` template wrapper string-ID handling (if it becomes a blocker for new service development).
- [ ] Add connection pool to Redis wrapper before load/performance testing.

## Blocked

- ODB 2.5.0 runtime build from source — not yet available in vcpkg; CMake fails at configure time if runtime is missing.
- Stale codec/gRPC generated files in `services/codec` — gated behind `MYCHAT_BUILD_CODEC_SERVICE=OFF`; must be regenerated with correct protoc/gRPC versions.
