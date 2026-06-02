---
task_id: task5
type: review
status: approved
from: planner_reviewer
to: orchestrator
revision: 0
decision: APPROVED
next_action: next_task
---

# Task 5 Review: Service Build Gating and Active Test Hygiene

## Decision

APPROVED

Task 5 meets the planned acceptance criteria:

- `MYCHAT_BUILD_SERVICES=ON` no longer pulls `im_codec_service` into the
  default build.
- User Service remains enabled when ODB is enabled.
- Fresh ODB and no-ODB builds complete through the default `cmake --build`.
- Fresh CTest runs only buildable active baseline tests.
- Legacy router/utils/network tests are gated behind
  `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.
- Codec service is behind `MYCHAT_BUILD_CODEC_SERVICE`.
- Docs and progress notes were updated.

## Findings

No blocking issues found.

Non-blocking follow-ups:

1. `services/user/user_service.cpp:111` now reverts the in-memory
   `last_login` when persistence fails, but login still returns `ok=true`.
   This is acceptable for the current service-core baseline, but Gateway-facing
   behavior should eventually distinguish "authenticated but state update
   failed" from a fully successful login.
2. `services/user/password_hasher.cpp:33` is stricter now, but malformed hash
   parser cases still do not have focused tests. Add pure unit tests before
   treating password handling as security-complete.
3. The current environment has gRPC available through the existing vcpkg prefix,
   so the explicit codec path was verified as a successful build. The
   no-gRPC `FATAL_ERROR` path was reviewed from CMake logic but not reproduced
   in this environment.

## Reviewer Verification

- Command: `docker compose up -d redis postgres`
  Result: Containers already running; PostgreSQL and Redis reported healthy.

- Command:
  `cmake -S . -B /tmp/mychat-task5-review-odb -DVCPKG_MANIFEST_FEATURES=pgsql-odb -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF -DMYCHAT_BUILD_PGSQL_ODB=ON -DCMAKE_BUILD_TYPE=Debug`
  Result: Passed. Configure output showed codec disabled and User Service enabled.

- Command:
  `cmake --build /tmp/mychat-task5-review-odb -j2`
  Result: Passed. Default build completed without building `im_codec_service`.

- Command:
  `ctest --test-dir /tmp/mychat-task5-review-odb --output-on-failure`
  Result: Passed, 4/4 tests:
  `RedisHiredisTest`, `ODBUserPersistenceTest`, `UserServiceCoreTest`,
  `AuthTokenTest`.

- Command:
  `cmake -S . -B /tmp/mychat-task5-review-no-odb -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF -DMYCHAT_BUILD_PGSQL_ODB=OFF -DCMAKE_BUILD_TYPE=Debug`
  Result: Passed. Configure output showed User Service skipped because ODB was
  unavailable.

- Command:
  `cmake --build /tmp/mychat-task5-review-no-odb -j2`
  Result: Passed.

- Command:
  `ctest --test-dir /tmp/mychat-task5-review-no-odb --output-on-failure`
  Result: Passed, 2/2 tests: `RedisHiredisTest`, `AuthTokenTest`.

- Command:
  `cmake -S . -B /tmp/mychat-task5-review-no-odb -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_CODEC_SERVICE=ON -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF -DMYCHAT_BUILD_PGSQL_ODB=OFF -DCMAKE_BUILD_TYPE=Debug`
  Result: Passed in this environment because gRPC was available.

- Command:
  `cmake --build /tmp/mychat-task5-review-no-odb --target im_codec_service -j2`
  Result: Passed. Explicit codec target built when explicitly enabled.

## Review Notes

The build surface is now much more predictable. The important improvement is
that `MYCHAT_BUILD_SERVICES=ON` means "enable the service area", not "force
every historical service artifact into the default build".

The implementation also fixed two Task 4 review follow-ups in a small,
compatible way. Those changes are not risky enough to block this task, but the
password parser should get focused unit tests later.

## Next Action

Proceed to Phase E: Gateway-to-User Service integration.
