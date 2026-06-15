---
task_id: task008
type: final
status: approved
from: orchestrator
to: project
revision: 0
decision: APPROVED
---

# Task 008 Plan: Extract PushService with FanoutPolicy Final Record

## Task

- Task ID: `task008`
- Decision: `APPROVED`

## Implementation Summary

---
task_id: task008
type: summary
status: ready_for_review
from: coder
to: reviewer
revision: 0
review_required: true
---

# Task 008 Summary: Extract PushService with FanoutPolicy

## Overview

Extracted the push logic from `MessageWsHandler::try_push_to_recipient` into a
dedicated `PushService` class with a pluggable `FanoutPolicy` abstraction.
`MessageWsHandler` now delegates to `PushService::push_to_user` instead of
owning the push loop. The default `AllSessionsFanoutPolicy` preserves existing
"push to all active sessions" behavior. `PushService` is independently testable.

## Files Changed

- `gateway/push_service.hpp` (new) — `FanoutPolicy` abstract interface,
  `AllSessionsFanoutPolicy` concrete implementation, `PushService` class with
  `push_to_user` and `set_fanout_policy`.
- `gateway/push_service.cpp` (new) — `PushService::push_to_user` replicates
  the push loop from `try_push_to_recipient`, with fanout policy selection.
  `AllSessionsFanoutPolicy::select_sessions` returns all session IDs.
- `gateway/message_ws_handler.hpp` — Replaced `ConnectionManager*` and
  `WebSocketServer*` members/params with `PushService*`. Added `#include
  "push_service.hpp"`. Removed `try_push_to_recipient` declaration and
  forward declarations for `ConnectionManager` and `WebSocketServer`.
- `gateway/message_ws_handler.cpp` — Removed `try_push_to_recipient`
  implementation. Removed direct includes for `connection_manager.hpp`,
  `websocket_server.hpp`, `push.pb.h`. Replaced push call with
  `if (push_service_) push_service_->push_to_user(...)`. Updated constructor.
- `gateway/CMakeLists.txt` — Added `push_service.cpp` to gateway library under
  Message Service build gate.
- `gateway/gateway_server/gateway_server.hpp` — Added `PushService` forward
  declaration and `std::unique_ptr<PushService> push_service_` member under
  `IM_ENABLE_MESSAGE_HTTP`.
- `gateway/gateway_server/gateway_server.cpp` — Creates `PushService` after
  `init_conn_mgr()` with `conn_mgr_.get()`, `websocket_server_.get()`, and a
  `MessageService`. Passes `push_service_.get()` to `MessageWsHandler`.
  Added `#include "../push_service.hpp"`.
- `test/gateway_message/test_gateway_message_ws.cpp` — Updated `MessageWsHandler`
  constructor to use `PushService*`. Updated UID prefixes to `task8-test-*`.
  Changed `SetUpConnectionManager` to `SetUpPushService` that creates a
  `PushService` with real `ConnectionManager`.
- `test/gateway_message/test_push_service.cpp` (new) — 4 test cases:
  `NullDepsReturnsSilently`, `NoActiveSessions`,
  `AllSessionsFanoutSelectsAll`, `CustomFanoutPolicyInjection`.
- `test/gateway_message/CMakeLists.txt` — Added `test_push_service` target.
- `docs/devlog/phase10_push_service_fanout.md` (new) — PushService design,
  FanoutPolicy abstraction, migration notes, test coverage, verification.
- `docs/devlog/current_progress.md` — Updated baseline, in-progress, next
  tasks, risks, and doc index.
- `docs/agent_context/roadmap.md` — Marked PushService with FanoutPolicy as
  complete in Phase F.
- `docs/agent_context/todo.md` — Added Task 008 to completed; updated
  current and next items.
- `docs/agent_context/tasks/task008/plan.md` — Task 008 plan.

## Behavior Changed

- `MessageWsHandler` constructor now takes `PushService*` (defaulting to
  nullptr) instead of `ConnectionManager*` and `WebSocketServer*`.
- The push loop is now owned by `PushService::push_to_user`, which uses
  `FanoutPolicy::select_sessions` to determine which sessions to push to.
- Default `AllSessionsFanoutPolicy` returns all session IDs, preserving the
  existing behavior from Tasks 007.
- `set_fanout_policy` allows custom fanout policies for testing or future
  device-preference policies.
- The sender's ack response is unchanged; push is best-effort and does not
  affect the ack.
- No changes to `CMD_SEND_MESSAGE` ack behavior, `MessageProcessor`,
  `ProcessorResult`, HTTP message routes, `MessageService`, or the no-ODB
  build gating.

## Key Design Decisions

1. **Separation of concerns**: `PushService` owns the push loop and fanout
   policy; `MessageWsHandler` only knows about `PushService` as a dependency.
   This decouples message handling from push delivery details.
2. **FanoutPolicy abstraction**: The `FanoutPolicy` interface allows injecting
   custom session-selection logic without modifying `PushService`. The default
   `AllSessionsFanoutPolicy` preserves existing behavior.
3. **GatewayServer owns PushService**: `GatewayServer` creates `PushService`
   after `init_conn_mgr()` and passes it to `MessageWsHandler`. This follows
   the same dependency-injection pattern used for auth and message services.
4. **Independent testability**: `PushServiceTest` tests push behavior without
   `MessageWsHandler` or the full gateway stack. `FanoutPolicy` can be tested
   in isolation.

## Verification

### ODB-enabled (9/9 passed)
```bash
cmake -S . -B /tmp/mychat-build-task008 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task008 -j2
ctest --test-dir /tmp/mychat-build-task008 --output-on-failure
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
| PushServiceTest | Passed |
| AuthTokenTest | Passed |

### No-ODB (2/2 passed)
```bash
cmake -S . -B /tmp/mychat-build-task008-noodb \
  -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-task008-noodb -j2
ctest --test-dir /tmp/mychat-build-task008-noodb --output-on-failure
```

| Test | Status |
|------|--------|
| RedisHiredisTest | Passed |
| AuthTokenTest | Passed |

## Deviations From Plan

- None. The implementation follows the plan as specified.

## Known Issues / Follow-Up

- Multi-recipient fanout for group messages requires a different `PushService`
  API shape (multiple receiver UIDs per call).
- Device-preference fanout policies (e.g., "push to mobile only") are future
  work enabled by the `FanoutPolicy` abstraction.
- `PushService` as a standalone microservice in `services/push/` is future
  work; currently lives in `gateway/`.
- Client-ACK-based delivery marking is deferred.
- `docs/agent_context/current_progress.md` was not present;
  `docs/devlog/current_progress.md` remains the active progress document.

## Review Summary

---
task_id: task008
type: review
status: approved
from: reviewer
to: orchestrator
revision: 0
decision: APPROVED
next_action: next_task
---

# Task 008 Review

## Decision

APPROVED

## Findings

No blocking issues found.

1. **Plan matches task** — `plan.md` frontmatter declares `task_id: task008`
   and the content correctly describes extracting `PushService` with
   `FanoutPolicy` from `MessageWsHandler`.

2. **Implementation matches plan** — `PushService` class owns the push loop
   with a pluggable `FanoutPolicy` abstract interface. Default
   `AllSessionsFanoutPolicy` preserves existing "push to all sessions"
   behavior. `MessageWsHandler` delegates to `PushService::push_to_user`
   instead of owning the push loop directly. Constructor signature
   simplified from two raw pointers (`ConnectionManager*`,
   `WebSocketServer*`) to a single `PushService*`.

3. **Tests pass** — 9/9 ODB-enabled tests passing, including the new
   `PushServiceTest` (4 cases: `NullDepsReturnsSilently`,
   `NoActiveSessions`, `AllSessionsFanoutSelectsAll`,
   `CustomFanoutPolicyInjection`) and the updated `GatewayMessageWsTest`.
   2/2 no-ODB tests passing.

4. **No stale artifacts** — Summary reports no `.invalid` files in the task
   directory.

5. **Documentation updated** — Phase 10 devlog
   (`docs/devlog/phase10_push_service_fanout.md`) covers PushService design,
   FanoutPolicy abstraction, migration notes, test coverage, and
   verification results. Roadmap, todo, and current progress updated.

6. **No scope creep** — The extraction is clean: no new push behavior
   introduced, no changes to `CMD_SEND_MESSAGE` ack, no changes to HTTP
   routes or `MessageService`. The plan's out-of-scope items (multi-recipient
   fanout, device-preference policies, standalone Push microservice,
   client-ACK delivery) remain deferred.

7. **No deviations from plan** — Summary reports zero deviations.

## Reviewer Verification

- Command: `head -5 docs/agent_context/tasks/task008/plan.md`
  Result: Frontmatter contains `task_id: task008`.

- Source review: `gateway/push_service.hpp`
  Result: `FanoutPolicy` abstract interface with `select_sessions` pure
  virtual; `AllSessionsFanoutPolicy` concrete default; `PushService` class
  with `push_to_user` and `set_fanout_policy`. Constructor takes
  `ConnectionManager*`, `WebSocketServer*`, and `MessageService`.

- Summary review: `PushService` constructor mirrors the old
  `try_push_to_recipient` dependencies exactly. `MessageWsHandler` now
  takes only `PushService*`. `GatewayServer` creates `PushService` after
  `init_conn_mgr()` and injects it. `PushServiceTest` has 4 independent
  test cases covering nullptr deps, no sessions, fanout selection, and
  custom policy injection.

- Command: `test -f docs/devlog/phase10_push_service_fanout.md && echo EXISTS`
  Result: EXISTS (per summary).

- Test results: 9/9 ODB (including new `PushServiceTest`), 2/2 no-ODB.

## Next Action

Continue to the next task.

## Next Action

Continue with the next planned task unless the review identified follow-up work.
