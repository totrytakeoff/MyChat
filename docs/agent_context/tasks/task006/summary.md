---
task_id: task006
type: summary
status: ready_for_review
from: coder
to: reviewer
revision: 2
review_required: true
---

# Task 006 Summary: Gateway WebSocket Message Send/Ack

## Overview

Added a tested `CMD_SEND_MESSAGE` (2001) WebSocket handler to the Gateway that
parses `im.message.SendMessageRequest`, persists the message through
`MessageService`, and returns an encoded `im.message.SendMessageResponse`
protobuf. The handler validates cmd_id, protobuf type name, access token,
device binding, receiver UID, and content before persisting. All expected
failure paths return a decodable protobuf error response.

## Changes

### New Files

- `gateway/message_ws_handler.hpp` — Header for `MessageWsHandler` class
- `gateway/message_ws_handler.cpp` — Implementation with `handle_send`:
  cmd_id check, token-based sender ID, protobuf type validation, content
  validation, persistence via `MessageService`, encoded protobuf success/error
  responses
- `test/gateway_message/test_gateway_message_ws.cpp` — 10 test cases covering
  success, token-derived identity, spoofed-from_uid rejection, invalid token,
  device mismatch, missing receiver, missing content, malformed payload, wrong
  protobuf type, wrong cmd_id
- `docs/devlog/phase8_gateway_message_ws_ack.md` — Devlog with protobuf
  contract, handler behavior, test coverage, verification results, remaining
  Phase F work

### Modified Files

- `gateway/CMakeLists.txt` — Added `message_ws_handler` sources to
  `im_gateway_core` under `IM_ENABLE_MESSAGE_HTTP` gate
- `gateway/gateway_server/gateway_server.hpp` — Added `MessageWsHandler`
  forward declaration and unique_ptr member under `IM_ENABLE_MESSAGE_HTTP`
- `gateway/gateway_server/gateway_server.cpp` — Instantiated
  `MessageWsHandler` and registered `CMD_SEND_MESSAGE` handler in
  `register_message_handlers()`
- `test/gateway_message/CMakeLists.txt` — Added `test_gateway_message_ws`
  target gated on `IM_ENABLE_MESSAGE_HTTP`
- `test/CMakeLists.txt` — Added `test/gateway_message` subdirectory
- `docs/devlog/current_progress.md` — Updated baseline, in-progress, next
  tasks, risks, and doc index
- `docs/agent_context/roadmap.md` — Added WebSocket send/ack as checked in
  Phase F
- `docs/agent_context/todo.md` — Added Task 006 to completed, updated current
  item
- Removed stale artifact `docs/agent_context/tasks/task006/summary.md.invalid`

## Key Design Decisions

1. **Sender identity**: Derived exclusively from the verified access token via
   `MultiPlatformAuthManager::verify_access_token`. Client-supplied
   `header.from_uid` and `SendMessageRequest.header.from_uid` are ignored.
2. **Protobuf type check**: The handler checks
   `UnifiedMessage::get_protobuf_type_name() == "im.message.SendMessageRequest"`
   before parsing. Wrong type names are rejected with `INVALID_REQUEST`.
3. **Cmd_id check**: The handler validates
   `msg.get_cmd_id() == im::command::CMD_SEND_MESSAGE` early. Mismatches are
   rejected with `INVALID_REQUEST`.
4. **Protobuf error responses**: All expected failure cases return an encoded
   `SendMessageResponse` with the appropriate error code in
   `BaseResponse.error_code`. The response header preserves the request
   sequence identity via `ProtobufCodec::returnHeaderBuilder`. This ensures the
   gateway server's WS response path (lines 609-616) sends the error to the
   client.
5. **Handler isolation**: `MessageWsHandler` is unit-testable without a live
   WebSocket server by constructing `UnifiedMessage` instances directly.
6. **HTTP route preservation**: Existing `GatewayMessageHttpTest` tests pass
   unchanged; HTTP message routes continue to work alongside the new WS handler.

## Build Gate

- ODB enabled: `test_gateway_message_ws` is built; `CMD_SEND_MESSAGE` handler
  is registered in gateway
- No ODB: Message Service, Message HTTP, and Message WS targets are all
  skipped cleanly

## Verification

All plan-required verification commands were run and pass:

### ODB-enabled (8/8 passed)
```bash
cmake -S . -B /tmp/mychat-build-task006 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task006 -j2
ctest --test-dir /tmp/mychat-build-task006 --output-on-failure
```

| Test | Status |
|------|--------|
| RedisHiredisTest | Passed |
| ODBUserPersistenceTest | Passed |
| UserServiceCoreTest | Passed |
| GatewayUserHttpTest | Passed |
| MessageServiceCoreTest | Passed |
| GatewayMessageHttpTest | Passed |
| GatewayMessageWsTest | Passed |
| AuthTokenTest | Passed |

### No-ODB (2/2 passed)
```bash
cmake -S . -B /tmp/mychat-build-task006-noodb \
  -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task006-noodb -j2
ctest --test-dir /tmp/mychat-build-task006-noodb --output-on-failure
```

| Test | Status |
|------|--------|
| RedisHiredisTest | Passed |
| AuthTokenTest | Passed |

## Out of Scope (as noted in plan)

- Online delivery to recipient's WebSocket sessions through `ConnectionManager`
- Delivered marking based on online push success
- Push Service implementation or fanout policy
- Full end-to-end live WebSocket client/server tests
- gRPC or codec regeneration
- Friend/group authorization
- Attachments, images, files, recalls, deletes
- Schema migrations
- Redis connection pooling
- `pgsql_conn.hpp` fixes
- HTTP Message route refactoring

## Remaining Phase F Work

- Online delivery through `ConnectionManager` (push to recipient's active WS
  sessions)
- Mark messages delivered on successful push
- Push Service implementation and fanout policy
- End-to-end WS integration tests
- Service-call strategy decision (direct integration vs codec/gRPC)

## Revision 2

### Changes

- Removed stale workflow artifact
  `docs/agent_context/tasks/task006/summary.md.invalid`.
- Rechecked the task artifact inventory; only `plan.md`, `review.md`, and
  `summary.md` remain in `docs/agent_context/tasks/task006/`.

### Verification

- Command: `git ls-files --others --exclude-standard docs/agent_context/tasks/task006`
  Result: Listed `plan.md`, `review.md`, and `summary.md` only.
- Command: `test -f docs/agent_context/tasks/task006/summary.md.invalid && echo INVALID_PRESENT || echo INVALID_ABSENT`
  Result: `INVALID_ABSENT`.

### Remaining Issues

- None.
