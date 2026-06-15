# Phase 12: Friend Service MVP Stabilization

## Summary

Added focused service-level and Gateway HTTP integration tests for the Friend
Service MVP. The Friend persistence model (im_friend_odb), repository, service
(im_friend_service), and Gateway HTTP controller (FriendHttpController) were
already present in the working tree from prior agent work and compiled/linked
successfully but had no tests. This phase adds deterministic test coverage to
stabilize the Friend MVP before larger group-message work builds on top.

## New Files

- `test/friend/CMakeLists.txt` — Google Test target for `FriendServiceCoreTest`.
- `test/friend/test_friend_service.cpp` — 13 service-level test cases.
- `test/gateway_friend/CMakeLists.txt` — Google Test target for `GatewayFriendHttpTest`.
- `test/gateway_friend/test_gateway_friend_http.cpp` — 16 Gateway HTTP test cases.

## Modified Files

- `test/CMakeLists.txt` — Added `test/friend` and `test/gateway_friend`
  subdirectories, gated on `im::friend_service` and `im::gateway_core`.

## Friend Service Test Coverage

### FriendServiceCoreTest (14 test cases)

| Test Case | What It Verifies |
|-----------|-----------------|
| SendFriendRequestHappyPath | Basic send request returns ok with "Friend request sent" message |
| SendFriendRequestEmptyUidRejected | Empty requester or target UID produces `EMPTY_UID` error |
| SendFriendRequestSelfRequestRejected | Self-request produces `SELF_REQUEST` error |
| SendFriendRequestDuplicatePairRejected | Same A→B pair rejected with `ALREADY_EXISTS` |
| SendFriendRequestDuplicateReverseRejected | Reverse B→A rejected with `ALREADY_EXISTS` |
| SendFriendRequestTargetNotFoundRejected | Nonexistent target_uid rejected with `TARGET_NOT_FOUND` |
| RespondToRequestOnlyTargetCanAccept | Requester gets `FORBIDDEN`; target can accept |
| RespondToRequestNonPendingRejected | Responding to already-accepted request gets `NOT_PENDING` |
| RespondToRequestNotFoundException | Nonexistent friend_id returns `NOT_FOUND` |
| RespondToRequestRejectUpdatesStatus | Rejecting returns ok with "Friend request rejected"; rejected not in friend list |
| GetAcceptedFriendsList | Two accepted friends appear in list with correct status/nickname/uid |
| GetPendingRequestsList | Target sees pending requests; requester sees empty |
| GetAcceptedFriendsReturnsEmptyForNoFriends | User with no friends gets empty list |
| GetPendingRequestsReturnsEmptyForNoRequests | User with no pending requests gets empty list |

### GatewayFriendHttpTest (16 test cases)

| Test Case | What It Verifies |
|-----------|-----------------|
| SendRequestMissingTokenReturns401 | No Authorization header → 401 |
| SendRequestInvalidTokenReturns401 | Invalid Bearer token → 401 |
| SendRequestInvalidJsonReturns400 | Non-JSON body → 400 |
| SendRequestMissingTargetUidReturns400 | JSON missing `target_uid` → 400 |
| RespondMissingFriendIdReturns400 | JSON missing `friend_id` → 400 |
| ListFriendsMissingTokenReturns401 | GET /api/v1/friends without auth → 401 |
| PendingRequestsMissingTokenReturns401 | GET /api/v1/friends/pending without auth → 401 |
| SendAndRespondHappyPath | Full flow: send → 201, accept → 200, both sides list friends → 200 |
| SendRequestSelfRequestRejected | target_uid == token uid → 400 |
| SendRequestTargetNotFoundReturns404 | Nonexistent target_uid → 404 |
| SendRequestDuplicateReturns409 | Same A→B twice → 409 |
| RespondOnlyForbiddenForNonTarget | Third party C responding to A→B → 403 |
| RespondNotFoundReturns404 | Nonexistent friend_id → 404 |
| PendingRequestsList | Target sees pending; requester does not |
| FriendsListReturnsEmptyForNoFriends | User with no friends → 200 with empty array |
| RoutesAreRegisteredAndHandleRequests | Real httplib server route test; all friend routes respond before catch-all |

## Verification

### ODB-enabled (11/11 passed):

```
RedisHiredisTest, ODBUserPersistenceTest, UserServiceCoreTest,
GatewayUserHttpTest, MessageServiceCoreTest, GatewayMessageHttpTest,
GatewayMessageWsTest, PushServiceTest, FriendServiceCoreTest,
GatewayFriendHttpTest, AuthTokenTest
```

### No-ODB (2/2 passed):

```
RedisHiredisTest, AuthTokenTest
```

## Friend Service API Contract

### send_request

Rejects with specific error codes in order of evaluation:

| Error Code | HTTP Status | Condition |
|-----------|-------------|-----------|
| `EMPTY_UID` | 400 | requester_uid or target_uid empty |
| `SELF_REQUEST` | 400 | requester_uid == target_uid |
| `TARGET_NOT_FOUND` | 404 | target_uid does not match any known user (via UserService::get_profile_by_uid) |
| `ALREADY_EXISTS` | 409 | A relationship (A→B or B→A) already exists in any status |
| `PERSIST_FAILED` | 500 | Database persist error |

Success: returns `ok=true`, HTTP 201, message "Friend request sent".

### respond_to_request

| Error Code | HTTP Status | Condition |
|-----------|-------------|-----------|
| `NOT_FOUND` | 404 | friend_id does not exist |
| `FORBIDDEN` | 403 | uid is not the target_uid of the friend request |
| `NOT_PENDING` | 400 | Friend request status is not PENDING (already accepted/rejected) |
| `UPDATE_FAILED` | 500 | Database update error |

Success: returns `ok=true`, HTTP 200, message "Friend request accepted" or
"Friend request rejected".

### get_friends / get_pending_requests

Always return HTTP 200 with `{"friends": [...]}` or `{"pending_requests": [...]}`.
Empty list when no matching records exist. The actor is always the authenticated
token UID.

## Constraints Observed

- No DROP TABLE in tests — cleanup by `DELETE WHERE ... LIKE 'friend-test-%'`
  and `'friend-http-test-%'`.
- No changes to codec/gRPC artifacts, PushService, MessageWsHandler ack, or
  default AllSessionsFanoutPolicy behavior.
- No refactoring of `pgsql_conn.hpp`.
- Friend tests use the same `test-prefixed` cleanup pattern as User and Message
  service tests.
- `friend_id` is obtained in tests via `get_pending_requests()` return value
  (the HTTP controller response for `send_request` does not return `friend_id`).
- The `register_friend_http_routes_on_server` free function (defined in
  `gateway_server.cpp`) is used as the test seam for route-layer integration
  tests, matching the User and Message HTTP test pattern.

## Build Configuration

### ODB-enabled:
```bash
cmake -S . -B /tmp/mychat-build-review \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review -j2
ctest --test-dir /tmp/mychat-build-review --output-on-failure
```

### No-ODB:
```bash
cmake -S . -B /tmp/mychat-build-review-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review-noodb -j2
ctest --test-dir /tmp/mychat-build-review-noodb --output-on-failure
```
