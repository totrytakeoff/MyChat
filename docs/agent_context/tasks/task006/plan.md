---
task_id: task006
type: plan
status: ready_for_implementation
from: planner
to: coder
revision: 0
requires_review: true
---

# Task 006 Plan: Gateway WebSocket Message Send/Ack

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 003 completed the Message Service persistence core. Task 004 completed
Gateway Message HTTP integration for authenticated send, history, and offline
pull. Task 005 was a stub workflow task and should not define the next real
engineering objective.

Durable context now shows Phase F still in progress. The remaining Phase F work
is WebSocket message send/ack, online delivery through `ConnectionManager`, Push
fanout, and eventual service-call strategy. The next smallest real development
task is to make the Gateway WebSocket `CMD_SEND_MESSAGE` path persist a direct
text message and return a protobuf ack. Online delivery/fanout is deliberately
kept for a later task because it requires exercising live WebSocket sessions and
`ConnectionManager` push behavior.

`docs/agent_context/current_progress.md` is not present in the repository. The
available current progress document is `docs/devlog/current_progress.md`, and
this plan also uses `docs/agent_context/roadmap.md`, `docs/agent_context/todo.md`,
Task 004 summary/review, and durable project context.

## Goal

Add a tested Gateway WebSocket send/ack handler for `CMD_SEND_MESSAGE` that
parses `im.message.SendMessageRequest`, persists the message through
`MessageService`, and returns an encoded `im.message.SendMessageResponse`.

## Relevant Files

- `gateway/CMakeLists.txt`
- `gateway/gateway_server/gateway_server.hpp`
- `gateway/gateway_server/gateway_server.cpp`
- `gateway/message_ws_handler.hpp`
- `gateway/message_ws_handler.cpp`
- `gateway/message_processor/unified_message.hpp`
- `gateway/message_processor/message_processor.hpp`
- `gateway/message_processor/message_processor.cpp`
- `services/message/message_service.hpp`
- `services/message/message_service.cpp`
- `common/proto/message.proto`
- `common/proto/message.pb.h`
- `common/proto/message.pb.cc`
- `common/proto/command.proto`
- `common/network/protobuf_codec.hpp`
- `common/network/protobuf_codec.cpp`
- `test/CMakeLists.txt`
- `test/gateway_message/CMakeLists.txt`
- `test/gateway_message/test_gateway_message_ws.cpp`
- `docs/devlog/phase8_gateway_message_ws_ack.md`
- `docs/devlog/current_progress.md`
- `docs/agent_context/roadmap.md`
- `docs/agent_context/todo.md`

## Required Behavior

- Gateway registers a real handler for `im.command.CMD_SEND_MESSAGE` (`2001`)
  when `im::message_service` is available.
- The handler accepts WebSocket `UnifiedMessage` values produced from protobuf
  envelopes and expects:
  - `cmd_id == CMD_SEND_MESSAGE`;
  - protobuf type name `im.message.SendMessageRequest`;
  - parseable `SendMessageRequest` payload bytes;
  - non-empty recipient UID from request/header;
  - non-empty text content.
- The persisted sender UID must come from the verified access token, not from
  client-supplied `header.from_uid` or request body data. If the implementation
  derives token identity inside the handler, it must call
  `MultiPlatformAuthManager::verify_access_token(token, UserTokenInfo&)` and
  reject mismatched `device_id` when present.
- Successful send persists through `MessageService::send_text_message` with
  `MessageType::TEXT`.
- Successful send returns a protobuf `SendMessageResponse` encoded with
  `ProtobufCodec::encode`.
- Response header preserves request sequence identity through the existing
  response-header builder pattern.
- Response body includes:
  - `BaseResponse.error_code = SUCCESS`;
  - persisted message ID as a string in `MessageBody.message_id`;
  - `MessageBody.type = TEXT`;
  - persisted content;
  - read/recalled flags reflecting current persisted state.
- Invalid token, invalid device binding, wrong protobuf type, malformed payload,
  missing receiver, missing content, or persistence failure returns a protobuf
  error response where possible and does not persist invalid data.
- Existing HTTP Message routes and no-ODB build gating must remain unchanged.

## Required Changes

- Add a small WebSocket message handler component, preferably
  `MessageWsHandler`, that is unit-testable without starting a real WebSocket
  server.
- Wire `MessageWsHandler` into `GatewayServer::register_message_handlers()` for
  `CMD_SEND_MESSAGE` under a Message Service build gate.
- Keep Message HTTP route registration behavior intact.
- Reuse the existing ODB database connection and Message Service initialization
  path where practical. Avoid duplicating PostgreSQL connection setup in
  multiple unrelated places.
- Do not trust `UnifiedMessage::get_from_uid()` as the sender identity for
  persistence.
- Add focused tests for the WebSocket handler. Tests may instantiate the handler
  directly and build `UnifiedMessage` instances from encoded protobuf payloads;
  they do not need to start a live TLS WebSocket server.
- Keep test database setup deterministic with `CREATE TABLE IF NOT EXISTS` and
  cleanup by task-prefixed sender/receiver UID only.
- Update durable context and devlog to show that WebSocket send/ack is complete,
  while online recipient fanout and Push Service remain pending.

## Verification

```bash
docker compose up -d redis postgres
cmake -S . -B /tmp/mychat-build-task006 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task006 -j2
ctest --test-dir /tmp/mychat-build-task006 -R "GatewayMessageWsTest|GatewayMessageHttpTest|MessageServiceCoreTest|AuthTokenTest" --output-on-failure
ctest --test-dir /tmp/mychat-build-task006 --output-on-failure
cmake -S . -B /tmp/mychat-build-task006-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task006-noodb -j2
ctest --test-dir /tmp/mychat-build-task006-noodb --output-on-failure
```

## Documentation Requirements

- Add `docs/devlog/phase8_gateway_message_ws_ack.md` with:
  - WebSocket request and response protobuf contract;
  - sender identity rule;
  - test coverage;
  - verification command results;
  - explicit remaining Phase F work.
- Update `docs/devlog/current_progress.md` if it remains the active current
  progress document.
- Update `docs/agent_context/roadmap.md` and `docs/agent_context/todo.md`
  without marking full Phase F complete.
- Note in the implementation summary that `docs/agent_context/current_progress.md`
  was absent unless it is created by context synchronization.

## Out Of Scope

- Pushing the persisted message to the recipient's online WebSocket sessions.
- Marking messages delivered based on online push success.
- Push Service implementation or fanout policy.
- Full end-to-end live WebSocket client/server tests.
- gRPC or `services/codec` regeneration.
- Friend authorization, block lists, or contact checks.
- Group messages.
- Attachments, images, files, recalls, deletes, and read-receipt workflows.
- Schema migrations.
- Redis connection pooling or performance work.
- Fixing `common/database/pgsql/pgsql_conn.hpp`.
- Refactoring HTTP Message routes beyond small shared helper extraction.

## Acceptance Criteria

The task is ready for review when:

- `GatewayMessageWsTest` covers successful protobuf send/ack, persisted message
  lookup, token-derived sender identity, spoofed `from_uid` rejection/ignore
  behavior, malformed payload, missing receiver/content, and wrong protobuf type.
- `CMD_SEND_MESSAGE` is registered in Gateway when Message Service is enabled.
- The WebSocket handler returns decodable `SendMessageResponse` protobuf bytes
  for success and decodable protobuf error responses for expected failures.
- Existing `GatewayMessageHttpTest`, `MessageServiceCoreTest`, and
  `AuthTokenTest` still pass in the ODB-enabled build.
- The no-ODB build still skips Message Service/WebSocket message handler targets
  cleanly.
- Roadmap, todo, and devlog clearly state that WebSocket send/ack is complete
  but online delivery through `ConnectionManager` and Push fanout remain pending.
