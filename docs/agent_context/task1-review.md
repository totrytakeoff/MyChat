# Task 1 Review

## Decision

APPROVED

## Findings

No blocking issues found.

Low-risk notes:

- `RedisClient::keys()` was added for test cleanup. This is acceptable for the
  current task because production code does not call it. Do not use Redis
  `KEYS` in production hot paths; prefer explicit indexes or `SCAN` if a
  production pattern iterator is ever needed.
- Existing unrelated warnings remain in network/router/message-parser code for
  unused callback parameters. They were present outside this task and should be
  cleaned in a separate warning-cleanup pass.

## Review Summary

The implementation follows the task plan:

- Refresh token metadata moved from the shared `refresh_tokens` hash to
  per-token keys: `refresh_token:<token>`.
- User refresh-token index moved to `user:<user_id>:refresh_tokens`.
- Access-token revocation moved from a shared Redis set to per-JTI keys:
  `revoked_access_token:<jti>`.
- Refresh token revoke/unrevoke now return `false` when the per-token key does
  not exist.
- Refresh token delete removes the per-token key and cleans the user index when
  metadata is available.
- Tests cover per-token key existence, legacy hash non-use, independent refresh
  token TTLs, wrong-device isolation, delete behavior, and per-JTI revoke/
  unrevoke behavior.
- Devlog documents the new Redis key layout and verification results.

## Reviewer Verification

Docker dependencies:

```bash
docker compose up -d redis postgres
docker compose ps
```

Result:

- `mychat-redis`: healthy
- `mychat-postgres`: healthy

Build:

```bash
cmake -S . -B /tmp/mychat-task1-review \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task1-review --target test_auth_tokens test_redis_hiredis gateway_server -j2
```

Result:

- Build succeeded.
- Only pre-existing unrelated unused-parameter warnings were observed.

Focused tests:

```bash
ctest --test-dir /tmp/mychat-task1-review -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 2
```

Gateway smoke:

```bash
/tmp/mychat-task1-review/gateway/gateway_server --config config/dev.json
curl http://127.0.0.1:8102/api/v1/health
```

Result:

```text
{"status": "ok"}
CURL:0 WAIT:0
```

## Next Recommended Task

Task 2 should verify and document the ODB toolchain, then start bringing the
PostgreSQL/ODB infrastructure into the staged build. Do not begin User Service
business logic until a minimal ODB persistence test passes against Docker
PostgreSQL.
