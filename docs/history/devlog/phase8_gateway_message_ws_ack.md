# Phase 8: Gateway WebSocket Message Send/Ack

## Protobuf Contract

- **Request**: `im.message.SendMessageRequest` (protobuf type name `im.message.SendMessageRequest`)
- **Response**: `im.message.SendMessageResponse`
- **Command ID**: `im.command.CMD_SEND_MESSAGE` = `2001`

### Request Fields

| Field | Source | Notes |
|-------|--------|-------|
| `header.token` | WS frame header | Access token for authentication |
| `header.from_uid` | WS frame header | **Ignored** — sender identity comes from verified token |
| `header.to_uid` | WS frame header | Receiver UID |
| `header.device_id` | WS frame header | Optional; must match token if present |
| `SendMessageRequest.body.content` | Protobuf payload | Message text content |
| `SendMessageRequest.header` | Protobuf payload | Nested header (ignored for identity) |

### Response Fields

| Field | Success | Error |
|-------|---------|-------|
| `BaseResponse.error_code` | `SUCCESS(0)` | Appropriate error code |
| `BaseResponse.error_message` | empty | Description of the error |
| `MessageBody.message_id` | Persisted message ID (string) | absent |
| `MessageBody.type` | `TEXT(0)` | absent |
| `MessageBody.content` | Persisted content | absent |
| `MessageBody.is_recalled` | `false` | absent |
| `MessageBody.is_read` | `false` | absent |

## Sender Identity Rule

The handler derives the sender UID from the verified access token
(`MultiPlatformAuthManager::verify_access_token`), **not** from any
client-supplied field such as `header.from_uid` or
`SendMessageRequest.header.from_uid`. If the device_id in the WS header does not
match the device_id embedded in the token, the request is rejected with
`AUTH_FAILED`.

## Handler Behavior

### Validation Order

1. Message protocol is WebSocket.
2. `cmd_id == CMD_SEND_MESSAGE` (2001).
3. Access token is valid and not expired.
4. Device ID in header matches token device ID (if header supplies one).
5. Protobuf type name is `im.message.SendMessageRequest`.
6. `SendMessageRequest` payload is parseable.
7. Receiver UID (`header.to_uid`) is non-empty.
8. Content (`body.content`) is non-empty.

### Error Handling

All expected failure paths return an encoded `SendMessageResponse` protobuf with
the appropriate error code set in `BaseResponse.error_code`. The response header
preserves the request sequence identity via `ProtobufCodec::returnHeaderBuilder`.

- **Non-WebSocket protocol**: `INVALID_REQUEST` (no protobuf body — gateway-level
  check, not emitted over WS)
- **Wrong cmd_id**: `INVALID_REQUEST`
- **Invalid/expired token**: `AUTH_FAILED`
- **Device ID mismatch**: `AUTH_FAILED`
- **Wrong protobuf type**: `INVALID_REQUEST`
- **Malformed payload**: `INVALID_REQUEST`
- **Missing receiver UID**: `PARAM_ERROR`
- **Missing content**: `PARAM_ERROR`
- **Persistence failure**: `SERVER_ERROR`

## Test Coverage

All tests in `GatewayMessageWsTest` (test/gateway_message/test_gateway_message_ws.cpp):

| Test | Verifies |
|------|----------|
| `SendWithValidTokenPersistsAndReturnsResponse` | Successful send persists message, returns decodable `SendMessageResponse` with correct fields |
| `SenderIdentityFromTokenNotHeader` | Token's `user_id` is used as sender, not `header.from_uid` |
| `InvalidTokenReturnsAuthFailed` | Invalid token → `AUTH_FAILED` protobuf error |
| `DeviceIdMismatchReturnsAuthFailed` | Mismatched device_id → `AUTH_FAILED` protobuf error |
| `MissingReceiverReturnsError` | Empty `to_uid` → `PARAM_ERROR` protobuf error |
| `MissingContentReturnsError` | Empty content → `PARAM_ERROR` protobuf error |
| `MalformedPayloadReturnsError` | Garbage bytes → `INVALID_REQUEST` protobuf error |
| `NonWebSocketMessageReturnsError` | HTTP message → `INVALID_REQUEST` (no protobuf body) |
| `WrongProtobufTypeRejected` | Wrong type name → `INVALID_REQUEST`; no message persisted |
| `WrongCmdIdRejected` | Wrong cmd_id → `INVALID_REQUEST`; no message persisted |

## Verification

All commands run and pass:

```bash
# ODB-enabled build — full suite
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
# Result: 100% passed, 0 failed out of 8

# No-ODB build — Message Service targets skipped
cmake -S . -B /tmp/mychat-build-task006-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task006-noodb -j2
ctest --test-dir /tmp/mychat-build-task006-noodb --output-on-failure
# Result: 100% passed, 0 failed out of 2
```

## Remaining Phase F Work

- Online delivery of persisted messages to recipient's active WebSocket sessions
  through `ConnectionManager`.
- Mark messages delivered based on online push success/failure.
- Push Service implementation or fanout policy.
- Full end-to-end live WebSocket client/server integration tests.
- Service-call strategy (direct integration vs. codec/gRPC regeneration).
