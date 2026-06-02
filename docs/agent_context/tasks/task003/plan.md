---
task_id: task003
type: plan
status: ready_for_implementation
from: planner
to: coder
revision: 0
requires_review: true
---

# Task 003 Plan: Message Service Persistence Core

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Phase E is complete: Gateway exposes register/login/profile HTTP routes backed
by the User Service MVP. The roadmap and todo now point to Phase F, Message
Service MVP, but that phase is too large for one implementation pass because it
includes service persistence, offline message workflows, history queries, and
Gateway online delivery.

This task is the next smallest real development task for Phase F: create the
Message Service core and prove direct-message persistence behavior with focused
tests. `docs/agent_context/current_progress.md` was referenced by the request
but is not present in the repository, so this plan uses `roadmap.md`, `todo.md`,
and the approved `task002` handoff as the current durable context.

## Goal

Add a buildable, tested Message Service core that can send/store one-to-one text
messages, query conversation history, and pull pending offline messages for a
recipient.

## Relevant Files

- `CMakeLists.txt`
- `services/CMakeLists.txt`
- `services/odb/CMakeLists.txt`
- `services/odb/message.hpp`
- `services/odb/generated/message-odb.cxx`
- `services/odb/generated/message-odb.hxx`
- `services/odb/generated/message-odb.ixx`
- `services/odb/generated/message.sql`
- `services/message/CMakeLists.txt`
- `services/message/message_repository.hpp`
- `services/message/message_repository.cpp`
- `services/message/message_service.hpp`
- `services/message/message_service.cpp`
- `test/CMakeLists.txt`
- `test/message/CMakeLists.txt`
- `test/message/test_message_service.cpp`
- `docs/devlog/phase6_message_service_core.md`
- `docs/agent_context/todo.md`
- `docs/agent_context/roadmap.md`

## Required Behavior

- Message data is represented by a new ODB object, separate from User Service
  persistence.
- The Message Service API supports direct text send, validation, chronological
  history query, offline pull, and delivered marking.
- Test setup must use `CREATE TABLE IF NOT EXISTS` and deterministic
  test-prefixed cleanup.
- The no-ODB build path must continue to configure/build without Message Service
  targets.

## Verification

```bash
docker compose up -d postgres
cmake -S . -B /tmp/mychat-build-task003 -DVCPKG_MANIFEST_FEATURES=pgsql-odb -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_PGSQL_ODB=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task003 -j2
ctest --test-dir /tmp/mychat-build-task003 -R "MessageServiceCoreTest|ODBUserPersistenceTest|UserServiceCoreTest|GatewayUserHttpTest|AuthTokenTest" --output-on-failure
```

## Out Of Scope

- Gateway HTTP/WebSocket route changes.
- Online delivery through `ConnectionManager`.
- Push Service behavior.
- Regenerating stale `services/codec` gRPC artifacts.
- Fixing `common/database/pgsql/pgsql_conn.hpp`.

## Acceptance Criteria

The task is ready for review when:

- `im_message_odb` and `im_message_service` build in the ODB-enabled staged
  configuration.
- The no-ODB configuration still builds and skips Message Service cleanly.
- `MessageServiceCoreTest` covers successful direct text send, validation,
  history query, offline pull, and delivered marking.
- Devlog and agent context updates accurately describe that only the Message
  Service persistence core is complete, not the full Phase F MVP.
