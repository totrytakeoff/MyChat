---
task_id: task007
type: plan
status: ready_for_implementation
from: planner
to: coder
revision: 0
requires_review: true
---

# Task 007 Plan: Gateway Online Message Delivery

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 003 completed the Message Service persistence core. Task 004 completed
Gateway Message HTTP integration. Task 006 completed Gateway WebSocket
`CMD_SEND_MESSAGE` send/ack handler. The remaining Phase F work is online
delivery to the recipient's active WebSocket sessions and marking messages
delivered after successful push.

The Gateway already has a `push_message_to_user` method in `GatewayServer`
that iterates recipient sessions from `ConnectionManager` and calls
`session->send()`, but it is not called from `MessageWsHandler::handle_send`.
The `ConnectionManager` tracks per-user device sessions in Redis, and
`WebSocketServer` provides live `SessionPtr` lookups by session ID.

The `im.push.PushRequest` protobuf (`common/proto/push.proto`) and
`CMD_PUSH_MESSAGE = 5001` (`common/proto/command.proto`) define the
server-to-client push contract.

## Goal

After `MessageWsHandler::handle_send` persists a message successfully, push
the message as a `CMD_PUSH_MESSAGE` protobuf to the recipient's active
WebSocket sessions via `ConnectionManager` and `WebSocketServer`, and mark
the message delivered after a successful push. Push is best-effort: if the
recipient has no active sessions, the message stays undelivered and will be
pulled later via the offline-pull API.

## Relevant Files

- `gateway/message_ws_handler.hpp`
- `gateway/message_ws_handler.cpp`
- `gateway/gateway_server/gateway_server.hpp`
- `gateway/gateway_server/gateway_server.cpp`
- `gateway/connection_manager/connection_manager.hpp`
- `gateway/connection_manager/connection_manager.cpp`
- `common/network/websocket_session.hpp`
- `common/network/websocket_server.hpp`
- `common/proto/push.proto`
- `common/proto/push.pb.h`
- `common/proto/push.pb.cc`
- `common/proto/command.proto`
- `common/network/protobuf_codec.hpp`
- `services/message/message_service.hpp`
- `services/message/message_service.cpp`
- `test/gateway_message/CMakeLists.txt`
- `test/gateway_message/test_gateway_message_ws.cpp`
- `test/CMakeLists.txt`
- `docs/devlog/phase9_gateway_online_delivery.md`
- `docs/devlog/current_progress.md`
- `docs/agent_context/roadmap.md`
- `docs/agent_context/todo.md`

## Required Behavior

- `MessageWsHandler` constructor accepts two additional parameters:
  `ConnectionManager*` and `network::WebSocketServer*`. Both may be nullptr
  in tests or when Message Service is built but ConnectionManager is not
  yet initialized.
- After successful persistence in `handle_send`, a new private helper
  `try_push_to_recipient` is called with the receiver UID, persisted message
  ID, and message content. The helper:
  1. Returns immediately if `conn_mgr_` or `ws_server_` is nullptr.
  2. Calls `conn_mgr_->get_user_sessions(receiver_uid)`.
  3. If no sessions are returned, logs and returns silently.
  4. For each `DeviceSessionInfo`, calls `ws_server_->get_session(session_id)`.
  5. Builds an `im.base.IMHeader` with `cmd_id = CMD_PUSH_MESSAGE (5001)`,
     `to_uid = receiver_uid`, `from_uid` set from
     `ServiceIdentityManager::getInstance().getDeviceId()`.
  6. Builds an `im.push.PushRequest` with `PushMessageBody` containing:
     - `type = PUSH_MESSAGE`
     - `content = message_text`
     - `related_message_id = std::to_string(msg_id)`
  7. Encodes via `ProtobufCodec::encode(header, push_req, encoded)`.
  8. Calls `session->send(encoded)` on each valid SessionPtr.
  9. After at least one session receives successfully, calls
     `msg_service_->mark_delivered(msg_id, now_ms())`.
  10. Logs push results (session count, success/failure).
- The existing `handle_send` control flow is unchanged: token verification,
  protobuf type check, persistence, and ack response all happen before the
  push attempt. The push does not affect the ack returned to the sender.
- Gateway server wires `MessageWsHandler` with `conn_mgr_.get()` and
  `websocket_server_.get()` so the push path is exercised in production.
- Push failures (missing session, encode failure, send exception) are
  caught individually per session and logged; they do not prevent pushing
  to other sessions or returning the sender's ack.
- Existing `GatewayMessageWsTest` cases continue to pass: the existing
  constructor signature is extended but all existing call sites pass
  `nullptr` for the new parameters, so the push path is silently skipped.
- No changes to `CMD_SEND_MESSAGE` ack behavior, `MessageProcessor`,
  `ProcessorResult`, or the protobuf response contract.
- No changes to existing HTTP message routes, `MessageService`, or the
  no-ODB build gating.

## Required Changes

- **`gateway/message_ws_handler.hpp`**: Add `#include` for
  `connection_manager.hpp` and `websocket_server.hpp`. Add
  `ConnectionManager* conn_mgr_` and `network::WebSocketServer* ws_server_`
  members. Add constructor parameters. Declare private
  `try_push_to_recipient` helper.
- **`gateway/message_ws_handler.cpp`**: Implement `try_push_to_recipient`
  with the push loop described above. Include `push.pb.h`. Call
  `try_push_to_recipient` from `handle_send` after successful persistence
  but before the return statement.
- **`gateway/gateway_server/gateway_server.hpp`**: Forward-declare
  `MessageWsHandler` already exists under `IM_ENABLE_MESSAGE_HTTP`; no
  header change required beyond what already exists.
- **`gateway/gateway_server/gateway_server.cpp`**: Update
  `MessageWsHandler` constructor call to pass `conn_mgr_.get()` and
  `websocket_server_.get()` (line 405 area).
- **`test/gateway_message/test_gateway_message_ws.cpp`**: Update existing
  `MessageWsHandler` constructor call to pass `nullptr, nullptr` for the
  new parameters. Add new test cases:
  1. Handler with ConnectionManager but no live sessions: persists message,
     push path is exercised without crash, message stays undelivered.
  2. Verify that null ConnectionManager gracefully skips push (existing
     tests implicitly cover this).
- **`test/gateway_message/CMakeLists.txt`**: If new tests need
  `ConnectionManager`, link against its library.
- **`docs/devlog/phase9_gateway_online_delivery.md`**: New devlog covering
  push contract, handler behavior, test coverage, and remaining Phase F
  work.
- **`docs/devlog/current_progress.md`**: Update baseline, in-progress, and
  next tasks.
- **`docs/agent_context/roadmap.md`**: Mark WebSocket online delivery as
  complete in Phase F.
- **`docs/agent_context/todo.md`**: Add Task 007 to completed; update
  current items (remaining: Push fanout, codec/gRPC decisions).

## Verification

```bash
docker compose up -d redis postgres
cmake -S . -B /tmp/mychat-build-task007 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task007 -j2
ctest --test-dir /tmp/mychat-build-task007 --output-on-failure
cmake -S . -B /tmp/mychat-build-task007-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task007-noodb -j2
ctest --test-dir /tmp/mychat-build-task007-noodb --output-on-failure
```

## Documentation Requirements

- Add `docs/devlog/phase9_gateway_online_delivery.md` with:
  - Push protobuf contract (CMD_PUSH_MESSAGE, PushRequest, PushMessageBody).
  - Handler push flow after persistence.
  - Best-effort delivery semantics.
  - Test coverage summary.
  - Verification command results.
  - Explicit remaining Phase F work (Push fanout, codec/gRPC decisions).
- Update `docs/devlog/current_progress.md`.
- Update `docs/agent_context/roadmap.md` and `docs/agent_context/todo.md`
  without marking Phase F complete.

## Out Of Scope

- Push fanout policy, Push Service implementation, or multi-device sync.
- Delivered marking based on client ACK (current implementation marks
  delivered on successful server-side push).
- Online/offline status notification (CMD_USER_ONLINE / CMD_USER_OFFLINE).
- Read receipts, message ACKs, or recall push notifications.
- Group message delivery or multi-recipient fanout.
- ConnectionManager or WebSocketServer interface changes.
- GatewayServer `push_message_to_user` refactoring or removal.
- End-to-end live WebSocket integration tests.
- gRPC/codec regeneration.
- Friend authorization or block lists.
- Schema migrations or Redis connection pooling.
- `pgsql_conn.hpp` fixes.
- `SendRequest::msg_type` default cleanup.

## Acceptance Criteria

The task is ready for review when:

- `MessageWsHandler` accepts `ConnectionManager*` and `WebSocketServer*`
  parameters, with nullptr-safe graceful degradation.
- After successful `CMD_SEND_MESSAGE` persistence, `try_push_to_recipient`
  pushes a `CMD_PUSH_MESSAGE` + `im.push.PushRequest` to each active
  recipient session and marks the message delivered after at least one
  successful push.
- Existing `GatewayMessageWsTest` cases continue to pass with nullptr
  ConnectionManager/WebSocketServer.
- New test cases cover: persistence with ConnectionManager but no live
  sessions (push path exercised without crash, message stays undelivered).
- All existing ODB-enabled tests (RedisHiredisTest, ODBUserPersistenceTest,
  UserServiceCoreTest, GatewayUserHttpTest, MessageServiceCoreTest,
  GatewayMessageHttpTest, GatewayMessageWsTest, AuthTokenTest) still pass.
- No-ODB build skips Message Service targets cleanly.
- Roadmap, todo, and devlog reflect online delivery completion while
  Phase F remains in progress.
