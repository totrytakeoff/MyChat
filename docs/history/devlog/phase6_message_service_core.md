# Phase 6 (partial): Message Service Persistence Core

## Goal

Add a buildable, tested Message Service persistence core that can send/store
one-to-one text messages, query conversation history, and pull pending offline
messages for a recipient. This is a subset of the full Phase F (Message Service
MVP), which also includes Gateway online delivery through ConnectionManager and
the Push path.

## Files Changed

### Created

- `services/odb/message.hpp` — Message ODB entity with `BIGSERIAL` auto-increment
  `msg_id`, sender/receiver UIDs, content, message type, status, timestamps,
  and `MessageStatus`/`MessageType` enums.
- `services/odb/generated/message-odb.cxx` — ODB compiler-generated object
  traits implementation.
- `services/odb/generated/message-odb.hxx` — ODB compiler-generated header.
- `services/odb/generated/message-odb.ixx` — ODB compiler-generated inline
  functions.
- `services/odb/generated/message.sql` — DDL for `im_messages` table creation.
- `services/message/message_repository.hpp` — Repository interface: `create`,
  `find_by_id`, `find_conversation`, `find_offline`, `mark_delivered`, `mark_read`.
- `services/message/message_repository.cpp` — ODB-backed implementation with
  explicit `ORDER BY create_time` for conversation and offline queries.
- `services/message/message_service.hpp` — Service API with `SendRequest`,
  `SendResult`, `MessageData` DTOs; methods: `send_text_message`,
  `get_conversation`, `pull_offline`, `mark_delivered`, `mark_read`.
- `services/message/message_service.cpp` — Business logic: validates sender,
  receiver, and content are non-empty; delegates to repository; converts
  entities to DTOs.
- `services/message/CMakeLists.txt` — Static library target `im_message_service`.
- `test/message/CMakeLists.txt` — GTest executable `test_message_service`.
- `test/message/test_message_service.cpp` — 8 test cases covering successful
  direct text send, empty-sender/receiver/content rejection, chronological
  conversation order (including non-chronological insert order), offline pull
  with ordering, delivered marking, read marking, and nonexistent-message
  rejection.

### Modified

- `CMakeLists.txt` — Added `MYCHAT_BUILD_MESSAGE_SERVICE` option (default ON,
  gated behind ODB availability).
- `services/CMakeLists.txt` — Gated `add_subdirectory(message)` on
  `TARGET im::message_odb`.
- `services/odb/CMakeLists.txt` — Added `im_message_odb` target mirroring
  `im_user_odb` pattern.
- `test/CMakeLists.txt` — Added `add_subdirectory(message)` gated on
  `TARGET im::message_service`.

## API (Service Layer)

| Method | Input | Output | Behavior |
|--------|-------|--------|----------|
| `send_text_message` | `SendRequest{sender_uid, receiver_uid, content, msg_type, now_ms}` | `SendResult{ok, error_code, message, data}` | Validates non-empty fields; persists message; returns created entity |
| `get_conversation` | `user_a, user_b, before_time, limit` | `vector<MessageData>` | Chronological history between two users (ordered by create_time ASC) |
| `pull_offline` | `receiver_uid, before_time, limit` | `vector<MessageData>` | Undelivered messages (status=SENT) for receiver (ordered by create_time ASC) |
| `mark_delivered` | `msg_id, delivered_time` | `bool` | Sets status=DELIVERED and delivered_time |
| `mark_read` | `msg_id, read_time` | `bool` | Sets status=READ and read_time |

Conversation and offline queries use `ORDER BY create_time` to guarantee
deterministic chronological results. Limits default to 50; `before_time`
defaults to `INT64_MAX` (fetch all).

## Build & Test

| Configuration | Tests | Status |
|---------------|-------|--------|
| ODB-enabled | 6 suites, 8 MessageServiceCore subtests | ✅ |
| No-ODB baseline | 3 suites | ✅ |

ODB-enabled build command:
```bash
cmake -S . -B /tmp/mychat-build-task003 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task003 -j2
ctest --test-dir /tmp/mychat-build-task003 --output-on-failure
```

Result: 6/6 tests passed (100%).

No-ODB build:
```bash
cmake -S . -B /tmp/mychat-build-task003-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task003-noodb -j2
```

Result: Build succeeded; Message Service targets correctly skipped.

## Scope Notes

This task implements only the **persistence core** of the Message Service MVP
(Phase F). The following remain as future work within Phase F:

- Gateway HTTP/WebSocket route changes for message send and delivery.
- Online delivery through `ConnectionManager`.
- Push Service fanout to online devices.
- Schema migration framework for PostgreSQL.
- Connection pooling for Redis and PostgreSQL.
