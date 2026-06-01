---
task_id: task6
type: review
status: changes_requested
from: planner_reviewer
to: implementation_agent
revision: 0
decision: CHANGES_REQUESTED
next_action: revise_same_task
---

# Task 6 Review: Gateway to User Service HTTP Integration

## Decision

CHANGES_REQUESTED

The implementation is not ready to commit as the Task 6 baseline. The core
controller idea is in the right direction, but the Gateway integration path is
currently not functional and the handoff is incomplete.

## Findings

1. **Blocking: Gateway user routes are never registered.**

   In `GatewayServer::init_server`, `init_http_server()` runs at
   `gateway/gateway_server/gateway_server.cpp:324`, but
   `user_http_controller_` is only created later at
   `gateway/gateway_server/gateway_server.cpp:346`.

   `init_http_server()` only registers user routes when
   `user_http_controller_` is already non-null
   (`gateway/gateway_server/gateway_server.cpp:739`). In the current order,
   it is null, so the server logs that user endpoints are disabled and only
   installs the legacy catch-all routes. This means the actual Gateway HTTP
   surface does not provide the new register/login/profile routes.

2. **Blocking: Implemented route paths do not match the Task 6 contract.**

   Task 6 requires:

   - `POST /api/v1/auth/register`
   - `POST /api/v1/auth/login`
   - `GET /api/v1/auth/info`

   The implementation registers:

   - `POST /api/v1/register`
   - `POST /api/v1/login`
   - `GET /api/v1/user/profile`

   See `gateway/gateway_server/gateway_server.cpp:740`,
   `gateway/gateway_server/gateway_server.cpp:744`, and
   `gateway/gateway_server/gateway_server.cpp:748`.

3. **Blocking: Unknown-account login maps to HTTP 500 instead of 401.**

   `UserService::login_by_account()` returns `INVALID_ACCOUNT` for a missing
   account, but `UserHttpController::handle_login()` checks
   `ACCOUNT_NOT_FOUND` at `gateway/user_http_controller.cpp:140`. A normal
   unknown-account login therefore falls through to the generic 500 path at
   `gateway/user_http_controller.cpp:144`.

4. **Blocking: Tests do not prove the Gateway HTTP integration.**

   `test/gateway_user/test_gateway_user_http.cpp` calls
   `UserHttpController` methods directly. That is useful controller coverage,
   but it does not catch the route-registration bug above, nor does it verify
   the required public paths. Task 6 requires a Gateway/User vertical slice; at
   least one test must exercise the registered Gateway HTTP routes or a
   route-registration unit seam that would fail with the current ordering.

5. **Blocking: Task handoff docs are missing.**

   `docs/agent_context/task6-summary.md` was not created, and the expected
   Phase 6 devlog/current-progress updates are also missing. The agent
   workflow requires the implementation summary before approval.

6. **Blocking: generated local build directory is left untracked.**

   `build-noodb/` is present in `git status`. Build output should not be left
   in the repository working tree. Remove it or ensure the relevant build
   artifact pattern is ignored.

## Required Fixes

- Create `docs/agent_context/task6-summary.md` and
  `docs/devlog/phase6_gateway_user_integration.md`.
- Initialize `user_http_controller_` before route registration, or refactor
  route registration so direct user routes are installed after the controller
  exists.
- Use the required paths:
  - `POST /api/v1/auth/register`
  - `POST /api/v1/auth/login`
  - `GET /api/v1/auth/info`
- Fix login error mapping for `INVALID_ACCOUNT`.
- Add tests that verify the Gateway route layer, not only direct controller
  methods.
- Remove `build-noodb/` from the working tree.
- Keep no-ODB builds green and either skip/register no User-backed Gateway
  tests clearly when `im::user_service` is unavailable.

## Reviewer Verification

- Command: `git diff --check`
  Result: Passed.

- Command: `cmake -S . -B /tmp/mychat-task6-review-odb ...`
  Result: Could not complete due `/tmp` disk quota during vcpkg manifest
  installation. This did not affect the review decision because the blocking
  issues above are visible by code inspection.

## Next Action

Revise the same task. Do not start the next task yet.
