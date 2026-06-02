---
task_id: task004
type: plan
status: ready_for_implementation
from: planner
to: coder
revision: 0
requires_review: true
---

# Task 004 Plan: Gateway Message HTTP Integration

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 003 is approved and completed the Message Service persistence core:
ODB-backed `im_messages`, validated one-to-one text send, chronological
conversation history, offline pull, and delivered/read marking. Durable roadmap
and todo context now show Phase F in progress, with Gateway delivery and Push
fanout still pending.

The next smallest development task is to expose the Message Service core through
Gateway HTTP routes using the same in-process MVP shortcut already used for
Gateway-to-User integration. This gives clients an authenticated Gateway path to
send text messages, query history, and pull offline messages without taking on
WebSocket online delivery or Push Service fanout yet.

`docs/agent_context/current_progress.md` is not present in the repository, so
this plan uses `roadmap.md`, `todo.md`, the Task 003 summary/review, and the
durable project context as the current progress source.

## Goal

Add authenticated Gateway HTTP endpoints for direct-message send, conversation
history, and offline message pull, backed by the existing Message Service.

## Relevant Files

- `gateway/CMakeLists.txt`
- `gateway/gateway_server/gateway_server.hpp`
- `gateway/gateway_server/gateway_server.cpp`
- `gateway/message_http_controller.hpp`
- `gateway/message_http_controller.cpp`
- `test/CMakeLists.txt`
- `test/gateway_message/CMakeLists.txt`
- `test/gateway_message/test_gateway_message_http.cpp`
- `docs/devlog/phase7_gateway_message_http.md`
- `docs/agent_context/roadmap.md`
- `docs/agent_context/todo.md`

## Required Behavior

- Gateway exposes Message HTTP routes only when both Gateway and Message Service targets are available.
- All Message HTTP routes require a valid Bearer access token.
- The authenticated token user is the sender/actor; client-supplied `sender_uid` is not trusted.
- `POST /api/v1/messages/send` accepts `receiver_uid` and `content`, then returns the persisted message.
- `GET /api/v1/messages/history` returns chronological messages with `peer_uid`.
- `GET /api/v1/messages/offline` returns pending offline messages and marks returned messages delivered.
- No-ODB builds continue to skip Message HTTP routes cleanly.

## Verification

```bash
docker compose up -d redis postgres
cmake -S . -B /tmp/mychat-build-task004 -DVCPKG_MANIFEST_FEATURES=pgsql-odb -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_PGSQL_ODB=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task004 -j2
ctest --test-dir /tmp/mychat-build-task004 -R "GatewayMessageHttpTest|MessageServiceCoreTest|GatewayUserHttpTest|AuthTokenTest" --output-on-failure
ctest --test-dir /tmp/mychat-build-task004 --output-on-failure
```

## Out Of Scope

- WebSocket message send/ack path.
- Online delivery through `ConnectionManager::push_message_to_user`.
- Push Service fanout.
- gRPC or `services/codec` regeneration.
- Friend authorization, group messages, attachments, schema migrations, Redis pooling, and `pgsql_conn.hpp` fixes.

## Acceptance Criteria

The task is ready for review when:

- `GatewayMessageHttpTest` covers authenticated send, invalid/missing token, invalid send body, history query, offline pull, delivered marking, and route registration before catch-all handlers.
- ODB-enabled builds include Message HTTP routes and focused Gateway/User/Message/Auth tests pass.
- No-ODB builds still skip Message Service and Message HTTP routes cleanly.
- Documentation and durable context state that Gateway HTTP message integration is complete while WebSocket online delivery and Push fanout remain pending.
