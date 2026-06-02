# Phase 10: PushService with FanoutPolicy

## Summary

Extracted the push logic from `MessageWsHandler::try_push_to_recipient` into a
dedicated `PushService` class with a pluggable `FanoutPolicy` abstraction.
`MessageWsHandler` now delegates to `PushService::push_to_user` instead of
owning the push loop directly.

## PushService Design

`PushService` (in `gateway/`) encapsulates the online push delivery path:

- Constructor accepts `ConnectionManager*`, `WebSocketServer*`, and
  `std::shared_ptr<MessageService>`. All may be nullptr.
- Holds a `std::unique_ptr<FanoutPolicy>` member; defaults to
  `AllSessionsFanoutPolicy`.
- `set_fanout_policy(std::unique_ptr<FanoutPolicy>)` allows injection of
  custom fanout policies for testing or future device-preference policies.
- `push_to_user(receiver_uid, msg_id, content)`:
  1. Returns silently if any dep is nullptr.
  2. Queries `ConnectionManager::get_user_sessions`.
  3. Calls `fanout_policy_->select_sessions(sessions)`.
  4. For each selected session, encodes `CMD_PUSH_MESSAGE` + `PushRequest`
     and calls `session->send(encoded)`.
  5. Per-session failures are caught individually.
  6. After at least one successful push, calls `mark_delivered`.
  7. Logs result summary.

## FanoutPolicy Abstraction

```cpp
class FanoutPolicy {
public:
    virtual ~FanoutPolicy() = default;
    virtual std::vector<std::string> select_sessions(
        const std::vector<DeviceSessionInfo>& sessions) = 0;
};
```

`AllSessionsFanoutPolicy` (default) returns all session IDs from the input.
This preserves the existing "push to all devices" behavior.

Custom policies can select a subset (e.g., mobile-only, newest-session-only)
without modifying `PushService` internals.

## Migration: MessageWsHandler Delegation

- `MessageWsHandler` constructor changed from
  `(msg_service, auth_mgr, conn_mgr, ws_server)` to
  `(msg_service, auth_mgr, push_service = nullptr)`.
- `try_push_to_recipient` removed; replaced with
  `if (push_service_) push_service_->push_to_user(...)`.
- `MessageWsHandler` no longer includes `connection_manager.hpp`,
  `websocket_server.hpp`, or `push.pb.h` directly.

## GatewayServer Wiring

- `GatewayServer` creates `PushService` after `init_conn_mgr()` with
  `conn_mgr_.get()`, `websocket_server_.get()`, and a `MessageService`.
- Passes `push_service_.get()` to `MessageWsHandler` constructor.

## Test Coverage

**GatewayMessageWsTest** (12 test cases):
- 10 send/ack tests unchanged from Tasks 006/007.
- `NullConnMgrSkipsPushGracefully` — handler with no PushService, push skipped.
- `ConnMgrWithNoSessionsStaysUndelivered` — handler with PushService, no sessions.

**PushServiceTest** (4 test cases):
- `NullDepsReturnsSilently` — PushService with nullptr deps, no crash.
- `NoActiveSessions` — PushService with real ConnectionManager, no sessions,
  message stays undelivered.
- `AllSessionsFanoutSelectsAll` — unit test, default policy returns all IDs.
- `CustomFanoutPolicyInjection` — custom `PlatformFilterFanoutPolicy`, verifies
  only matching sessions selected.

## Verification

ODB-enabled (9/9 passed):
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

No-ODB (2/2 passed):
| Test | Status |
|------|--------|
| RedisHiredisTest | Passed |
| AuthTokenTest | Passed |

## Remaining Fanout/Push Work

- Device-preference fanout policies (e.g., push to mobile only, newest session).
- Multi-recipient fanout for group messages.
- `PushService` as a standalone microservice in `services/push/`.
- Client-ACK-based delivery marking.
- Full end-to-end live WebSocket integration tests.
