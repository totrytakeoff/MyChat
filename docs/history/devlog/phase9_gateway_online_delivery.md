# Phase 9: Gateway Online Message Delivery

## Summary

After `MessageWsHandler::handle_send` persists a message successfully, the
handler now pushes the message as a `CMD_PUSH_MESSAGE` protobuf to the
recipient's active WebSocket sessions via `ConnectionManager` and
`WebSocketServer`, and marks the message delivered after at least one
successful push.

## Push Protobuf Contract

- **Command**: `im.command.CMD_PUSH_MESSAGE` (5001)
- **Protobuf type**: `im.push.PushRequest`
- **PushMessageBody fields**:
  - `type = PUSH_MESSAGE`
  - `content = message_text`
  - `related_message_id = std::to_string(msg_id)`
- **IMHeader fields**:
  - `cmd_id = CMD_PUSH_MESSAGE (5001)`
  - `from_uid = ServiceIdentityManager::getInstance().getDeviceId()`
  - `to_uid = receiver_uid`

## Handler Behavior

1. `MessageWsHandler` constructor now accepts `ConnectionManager*` and
   `network::WebSocketServer*` (both default to nullptr).
2. After successful persistence, `try_push_to_recipient` is called with
   the receiver UID, persisted message ID, and message content.
3. The helper:
   - Returns immediately if `conn_mgr_` or `ws_server_` is nullptr.
   - Calls `conn_mgr_->get_user_sessions(receiver_uid)`.
   - If no sessions returned, logs and returns silently.
   - For each session, calls `ws_server_->get_session(session_id)`.
   - Builds a `CMD_PUSH_MESSAGE` + `PushRequest` envelope.
   - Encodes via `ProtobufCodec::encode`.
   - Calls `session->send(encoded)` on each valid SessionPtr.
   - Per-session exceptions are caught and logged individually.
   - After at least one session receives successfully, calls
     `msg_service_->mark_delivered(msg_id, now_ms())`.
4. The push does not affect the sender's ack response.

## Best-Effort Delivery Semantics

- If the recipient has no active sessions, the message stays undelivered
  and will be pulled later via the offline-pull API.
- Push failures (missing session, encode failure, send exception) are
  caught per session and do not prevent pushing to other sessions.
- The sender always receives a successful ack regardless of push outcome.

## Test Coverage

`GatewayMessageWsTest` includes 12 test cases:

1. SendWithValidTokenPersistsAndReturnsResponse
2. SenderIdentityFromTokenNotHeader
3. InvalidTokenReturnsAuthFailed
4. DeviceIdMismatchReturnsAuthFailed
5. MissingReceiverReturnsError
6. MissingContentReturnsError
7. MalformedPayloadReturnsError
8. NonWebSocketMessageReturnsError
9. WrongProtobufTypeRejected
10. WrongCmdIdRejected
11. NullConnMgrSkipsPushGracefully - handler with no ConnectionManager
    persists and acks; message stays undelivered.
12. ConnMgrWithNoSessionsStaysUndelivered - handler with ConnectionManager
    but no online sessions; message stays undelivered.

## Verification

ODB-enabled (8/8 passed):
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

No-ODB (2/2 passed):
```bash
cmake -S . -B /tmp/mychat-build-task007-noodb \
  -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task007-noodb -j2
ctest --test-dir /tmp/mychat-build-task007-noodb --output-on-failure
```

| Test | Status |
|------|--------|
| RedisHiredisTest | Passed |
| AuthTokenTest | Passed |

## Remaining Phase F Work

- Push Service implementation and fanout policy.
- Delivered marking based on client ACK (current implementation marks
  delivered on successful server-side push).
- End-to-end live WebSocket integration tests.
- Service-call strategy decision (direct integration vs codec/gRPC).
