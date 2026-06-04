# Phase 11: Production Fanout Policies

## Summary

Added two production FanoutPolicy implementations to PushService:

- PlatformFilterFanoutPolicy - selects sessions whose platform matches one
  of the allowed platforms. Typical use: push only to mobile devices, or only
  to web clients.
- NewestSessionFanoutPolicy - selects the single most recently connected
  session. Useful for notification-type pushes that only need to reach one
  device.

Both policies now live in `gateway/push/fanout_policies.hpp` and
`gateway/push/fanout_policies.cpp`, gated behind `im::message_service`
availability (same as PushService itself).

The default AllSessionsFanoutPolicy behavior is unchanged.

## New Files

- gateway/push/fanout_policies.hpp - declarations for PlatformFilterFanoutPolicy
  and NewestSessionFanoutPolicy.
- gateway/push/fanout_policies.cpp - implementations.

## Modified Files

- gateway/CMakeLists.txt - added `push/fanout_policies.cpp` to im_gateway_core
  sources when im::message_service is available.
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
