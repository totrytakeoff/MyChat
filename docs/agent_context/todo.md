---
type: todo
status: active
updated_by: coder
---

# Todo

## Completed

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

## Current

- [ ] Message Service MVP (Phase F) - Multi-recipient fanout, Push Service
  as standalone microservice, and service-call strategy. Persistence core
  (Task 003), Gateway HTTP integration (Task 004), WebSocket send/ack
  (Task 006), online delivery (Task 007), and PushService with FanoutPolicy
  (Task 008) are complete.

## Next

- [ ] Multi-recipient fanout for group messages and device-preference policies.
- [ ] Regenerate codec/gRPC artifacts (prerequisite for Message Service inter-service calls, or decide to follow same direct-integration pattern).
- [ ] Fix `pgsql_conn.hpp` template wrapper string-ID handling (if it becomes a blocker for new service development).
- [ ] Add connection pool to Redis wrapper before load/performance testing.

## Blocked

- ODB 2.5.0 runtime build from source — not yet available in vcpkg; CMake fails at configure time if runtime is missing.
- Stale codec/gRPC generated files in `services/codec` — gated behind `MYCHAT_BUILD_CODEC_SERVICE=OFF`; must be regenerated with correct protoc/gRPC versions.
