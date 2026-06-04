# Phase 11: Production Fanout Policies

## Summary

Added two production FanoutPolicy implementations to PushService:

- PlatformFilterFanoutPolicy - selects sessions whose platform matches one
  of the allowed platforms. Typical use: push only to mobile devices, or only
  to web clients.
- NewestSessionFanoutPolicy - selects the single most recently connected
  session. Useful for notification-type pushes that only need to reach one
  device.

Both policies originally lived in `gateway/push/fanout_policies.*`. They were
later migrated into `services/push/fanout_policy.hpp` and
`services/push/fanout_policy.cpp` so service-owned fanout selection is no
longer tied to Gateway runtime code.

The default AllSessionsFanoutPolicy behavior is unchanged.

## New Files

- services/push/fanout_policy.hpp - declarations for `PushSessionInfo`,
  `FanoutPolicy`, `AllSessionsFanoutPolicy`, `PlatformFilterFanoutPolicy`, and
  `NewestSessionFanoutPolicy`.
- services/push/fanout_policy.cpp - implementations.

## Modified Files

- services/push/CMakeLists.txt - builds `im_push_service` with fanout policy
  implementation.
- gateway/CMakeLists.txt - links `im_gateway_core` against `im::push_service`
  when available; Gateway no longer compiles fanout policy sources directly.
- test/gateway_message/test_push_service.cpp - removed the test-local
  PlatformFilterFanoutPolicy class, replaced with the production import.
  Added a make_session helper. Expanded from 4 to 11 test cases.

## Test Coverage

PushServiceTest (11 test cases):

Existing (4):
- NullDepsReturnsSilently
- NoActiveSessions
- AllSessionsFanoutSelectsAll
- CustomFanoutPolicyInjection (now uses production PlatformFilterFanoutPolicy)

New (7):
- PlatformFilterSingleMatch - filter to one platform, only matching session selected.
- PlatformFilterMultiplePlatforms - filter to two platforms, both matching sessions selected.
- PlatformFilterNoMatch - filter to a platform with no sessions, returns empty.
- PlatformFilterEmptyAllowedList - empty allowed list, returns empty.
- NewestSessionSelectsLatest - three sessions with different connect times, selects newest.
- NewestSessionSingleSession - single session, returns that one.
- NewestSessionNoSessions - empty input, returns empty.

## Verification

ODB-enabled (9/9 passed):
  RedisHiredisTest, ODBUserPersistenceTest, UserServiceCoreTest,
  GatewayUserHttpTest, MessageServiceCoreTest, GatewayMessageHttpTest,
  GatewayMessageWsTest, PushServiceTest, AuthTokenTest

No-ODB (2/2 passed):
  RedisHiredisTest, AuthTokenTest
