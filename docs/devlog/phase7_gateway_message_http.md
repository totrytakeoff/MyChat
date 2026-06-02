# Phase 7: Gateway Message HTTP Integration

## Goal

Expose the Message Service persistence core (Phase 6/task003) through
authenticated Gateway HTTP routes, following the same in-process MVP shortcut
already used for Gateway-to-User integration (Phase 6). This gives clients a
REST path to send text messages, query conversation history, and pull offline
messages without taking on WebSocket online delivery or Push fanout.

## Files Changed

### Created

- `gateway/message_http_controller.hpp` — Controller class with
  `handle_send`, `handle_history`, `handle_offline` methods. All endpoints
  require a valid Bearer access token.
- `gateway/message_http_controller.cpp` — Implementation: Bearer token
  extraction, `UserTokenInfo` verification, JSON request/response, delegates to
  `MessageService`.
- `test/gateway_message/CMakeLists.txt` — Test target linking `im::gateway_core`,
  `im::gateway_auth`, `im::message_service`.
- `test/gateway_message/test_gateway_message_http.cpp` — 9 tests: authenticated
  send (201), missing/invalid token (401), missing fields (400), token-UID trust
  (body `sender_uid` ignored), history query, missing peer_uid (400), offline
  pull with auto-deliver marking, and full HTTP route-layer integration test.
- `docs/devlog/phase7_gateway_message_http.md` — This file.

### Modified

- `gateway/gateway_server.hpp` — Added forward declarations for
  `MessageService` and `MessageHttpController` under `IM_ENABLE_MESSAGE_HTTP`;
  `register_message_http_routes()` member function; `message_http_controller_`
  member.
- `gateway/gateway_server.cpp` — Added free function
  `register_message_http_routes_on_server()` as a unit-testable seam; created
  controller in `init_server()` before `init_http_server()`; registered routes
  before catch-all handlers; added `register_message_http_routes()` member.
- `gateway/CMakeLists.txt` — Conditional source/link/define when
  `TARGET im::message_service`.
- `test/CMakeLists.txt` — Adds `gateway_message` when both `im::message_service`
  and `im::gateway_core` exist.

## HTTP API

| Method | Path | Auth | Status | Response |
|--------|------|------|--------|----------|
| POST | `/api/v1/messages/send` | Bearer | 201 | `{message: {msg_id, sender_uid, receiver_uid, content, ...}}` |
| GET | `/api/v1/messages/history?peer_uid=X` | Bearer | 200 | `{messages: [...], count: N}` |
| GET | `/api/v1/messages/offline` | Bearer | 200 | `{messages: [...], count: N}` |

- All endpoints require `Authorization: Bearer <access-token>`.
- The authenticated token user is the sender/actor; client-supplied
  `sender_uid` in the request body is ignored.
- `POST /api/v1/messages/send` accepts `receiver_uid` and `content`.
- `GET /api/v1/messages/history` returns chronological messages with `peer_uid`.
- `GET /api/v1/messages/offline` returns pending offline messages and
  automatically marks returned messages as delivered.
- Error responses: 400 (missing fields/params), 401 (missing/invalid token),
  500 (internal error).

## Route Registration Architecture

Message routes follow the same pattern as user routes:

1. `init_server()` creates `MessageHttpController` before `init_http_server()`.
2. `init_http_server()` registers health check, user routes, then message
   routes, then catch-all handlers.
3. The free function `register_message_http_routes_on_server()` is declared in
   the global namespace, serving as a unit-testable seam.

## Build & Test

| Configuration | Tests | Status |
|---------------|-------|--------|
| ODB-enabled | 7 suites, 9 GatewayMessageHttp subtests | ✅ |
| No-ODB baseline | (no message/HTTP targets) | ✅ |

Full test command:
```bash
docker compose up -d redis postgres
cmake -S . -B /tmp/mychat-build-task004 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task004 -j2
ctest --test-dir /tmp/mychat-build-task004 --output-on-failure
```

Result: 7/7 tests passed (100%).

## Scope Notes

This task completes Gateway HTTP message integration. The following remain as
future work within Phase F:

- WebSocket message send/ack path.
- Online delivery through `ConnectionManager::push_message_to_user`.
- Push Service fanout for online delivery.
- Schema migration framework for PostgreSQL.
- Connection pooling for Redis and PostgreSQL.
