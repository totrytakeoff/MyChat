---
task_id: task4
type: review
status: approved
from: planner_reviewer
to: orchestrator
revision: 0
decision: APPROVED
next_action: next_task
---

# Task 4 Review: User Service Core MVP Baseline

## Decision

APPROVED

Task 4 meets the planned acceptance criteria for the User Service core MVP:

- `im_user_service` is a deterministic target under `services/user`.
- Register/login/profile behavior is implemented on top of ODB/PostgreSQL.
- Passwords are persisted as PBKDF2-HMAC-SHA256 hashes, not plaintext.
- Focused tests cover duplicate account rejection, successful login,
  wrong-password login, profile lookup, and `last_login` persistence.
- Docs and progress notes were updated.

## Findings

No blocking issues found for the Task 4 scope.

Non-blocking issues to carry forward:

1. `services/CMakeLists.txt:3` still builds `im_codec_service`
   unconditionally when `MYCHAT_BUILD_SERVICES=ON`. A full build of the
   ODB-enabled tree fails on stale codec/gRPC artifacts:
   `grpcpp/generic/async_generic_service.h: No such file or directory`.
   This predates the User Service work, but it means "build all services" is
   not yet a clean developer path. Fix this before or at the start of Phase E.
2. `services/user/user_service.cpp:110` ignores the return value of
   `repo_->update_last_login(...)`. Normal-path tests prove the update works,
   but a later DB failure after password verification would still return a
   successful login with an in-memory `last_login`. Tighten this before
   exposing the flow through Gateway.
3. `services/user/password_hasher.cpp:32` accepts hex through `std::strtol`.
   It works for hashes produced by this implementation, but it is permissive
   for malformed input and does not reject extra trailing fields after the
   stored hash. Add stricter parser tests before raising password handling to a
   production/security baseline.

## Reviewer Verification

- Command: `docker compose up -d redis postgres`
  Result: Redis and PostgreSQL containers started; PostgreSQL reported healthy
  through container `pg_isready`.

- Command:
  `cmake --build /tmp/mychat-task4-review-user2 --target im_user_service test_user_service test_auth_tokens test_redis_hiredis test_odb_user_persistence -j2`
  Result: Passed.

- Command:
  `ctest --test-dir /tmp/mychat-task4-review-user2 -R 'RedisHiredisTest|ODBUserPersistenceTest|UserServiceCoreTest|AuthTokenTest' --output-on-failure`
  Result: Passed, 4/4 tests.

- Command:
  `cmake --build /tmp/mychat-task4-review-no-odb2 --target test_auth_tokens test_redis_hiredis gateway_server -j2`
  Result: Passed.

- Command:
  `ctest --test-dir /tmp/mychat-task4-review-no-odb2 --output-on-failure`
  Result: Passed, 2/2 tests.

- Command:
  Standalone compile checks for `services/user/user_service.hpp` and
  `services/user/user_repository.hpp`.
  Result: Passed.

- Extra command:
  `cmake --build /tmp/mychat-task4-review-user2 -j2`
  Result: Failed in pre-existing `im_codec_service` because generated gRPC
  code includes `grpcpp/generic/async_generic_service.h`, which is not
  available in the current dependency set. This is recorded as a follow-up
  service-build hygiene task, not as a Task 4 rejection.

## Review Notes

The implementation correctly avoids the broken `PgSqlConnection` wrapper and
uses `odb::pgsql::database` directly, which matches the plan. The repository
surface is small enough to replace or wrap later if the common PostgreSQL
abstraction is repaired.

The tests are integration-style unit tests against Docker PostgreSQL. That is
acceptable for this stage because the goal is proving the ODB-backed User
Service vertical slice. Keep this pattern for persistence-sensitive behavior,
but introduce smaller pure unit tests around parsing and validation logic when
the service grows.

## Next Action

Proceed to the next task. Recommended next task:

1. Clean service build gating so `MYCHAT_BUILD_SERVICES=ON` no longer pulls in
   stale codec/gRPC targets unless their dependencies are explicitly enabled.
2. Then start Phase E: Gateway-to-User Service integration for
   register/login/profile routes.
