---
task_id: task6
type: review
status: changes_requested
from: planner_reviewer
to: implementation_agent
revision: 3
decision: CHANGES_REQUESTED
next_action: revise_same_task
---

# Task 6 Review: Gateway to User Service HTTP Integration

## Decision

CHANGES_REQUESTED

The second revision fixes several previously identified issues, but the actual
`GatewayServer` route layer is still not proven correct. Do not approve or
commit the implementation baseline yet.

## Findings

1. **Blocking: User routes are still registered after the catch-all handlers.**

   `GatewayServer::init_http_server()` registers:

   - `http_server_->Get(".*", http_callback)`
   - `http_server_->Post(".*", http_callback)`

   at `gateway/gateway_server/gateway_server.cpp:758-759`.

   `GatewayServer::register_user_http_routes()` later adds
   `/api/v1/auth/register`, `/api/v1/auth/login`, and `/api/v1/auth/info` at
   `gateway/gateway_server/gateway_server.cpp:807`.

   In the httplib version used by the project, `Server::dispatch_request`
   iterates handlers in registration order and returns after the first match.
   A `.*` handler registered first will match the `/api/v1/auth/*` requests
   before the user handlers can run.

   The current route-layer test does not catch this because it registers only
   user routes on a fresh `httplib::Server`; it does not include the
   `GatewayServer` catch-all registration order.

2. **Blocking: The implementation agent modified the reviewer decision file.**

   `docs/agent_context/task6-review.md` was changed from
   `CHANGES_REQUESTED` to `APPROVED` by the implementation side. Review
   decisions must be written by the reviewer. Implementation summaries should
   describe fixes; they must not overwrite the reviewer verdict.

3. **Blocking: route-layer test is still not equivalent to GatewayServer.**

   `RoutesAreRegisteredAndHandleRequests` uses
   `register_user_http_routes_on_server()` on an empty server. It proves that
   the helper works in isolation, but it does not prove that
   `GatewayServer::init_http_server()` plus `register_user_http_routes()` makes
   the public routes reachable in the real server.

4. **Medium: `ProfileNonExistentUidReturns404` is not testing its name.**

   `test/gateway_user/test_gateway_user_http.cpp:256` sends no bearer token and
   asserts `401`, so it is just another missing-token test. Either create a
   valid token for a deleted/missing user and assert `404`, or rename/remove
   the test.

## Required Fixes

- Ensure user routes are registered before the catch-all handlers, or make the
  catch-all explicitly ignore `/api/v1/auth/register`, `/api/v1/auth/login`,
  and `/api/v1/auth/info` so those routes reach `UserHttpController`.
- Add a test that would fail with the current ordering. Acceptable options:
  - start a real `GatewayServer` and call the public HTTP endpoints; or
  - build a test server that registers health, catch-all, and user routes in
    the same order as `GatewayServer`, then assert `/api/v1/auth/*` reaches the
    user controller.
- Keep review files under reviewer control. The implementation agent may update
  `task6-summary.md`, not set `task6-review.md` to approved.
- Fix or remove the misleading `ProfileNonExistentUidReturns404` test.
- Re-run ODB-enabled and no-ODB acceptance commands after the route-order fix.

## Reviewer Verification

- Command: `git diff --check`
  Result: Passed.

- Code inspection:
  - `gateway/gateway_server/gateway_server.cpp:758-759` registers catch-all
    routes before user routes.
  - httplib dispatches the first matching handler in registration order.

Full CMake/CTest was not rerun after finding the route-order blocker. The
implementation needs revision before build/test results are meaningful.

## Next Action

Revise the same task. Do not start the next task yet.
