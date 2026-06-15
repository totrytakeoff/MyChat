# Task 1 Plan: Auth Redis Token Storage Hardening

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Gateway/Auth is the first service MVP hardening target. The current
`MultiPlatformAuthManager` has focused token tests, but its Redis token storage
model is still too coarse:

- refresh tokens are fields inside one shared Redis hash named
  `refresh_tokens`;
- `EXPIRE refresh_tokens ...` applies to all refresh tokens at once;
- revoked access token JTIs are stored in one Redis set named
  `revoked_access_tokens` with no per-token TTL.

This task fixes that storage model without expanding into User Service,
PostgreSQL, ODB, or Gateway endpoint work.

Relevant files:

- `gateway/auth/multi_platform_auth.cpp`
- `gateway/auth/multi_platform_auth.hpp`
- `test/auth/test_auth_tokens.cpp`
- `common/database/redis/redis_mgr.hpp`
- `common/database/redis/redis_mgr.cpp`
- `docs/devlog/current_progress.md`
- `docs/devlog/phase1_gateway_auth_baseline.md`

## Goal

Move Auth token Redis state from shared collections to per-token keys with
independent TTL where appropriate.

## Required Behavior

### Refresh Token Storage

Use per-token metadata keys:

```text
refresh_token:<refresh_token>
```

The value should be the existing JSON metadata currently stored as the hash
field value.

Each `refresh_token:<refresh_token>` key must have its own TTL based on the
actual refresh token expiry.

Keep a user index:

```text
user:<user_id>:refresh_tokens
```

The index should contain refresh token strings for that user. It may have a TTL
matching the token expiry for now, but correctness must not depend on this set
being the source of token validity. The per-token key is the source of truth.

### Refresh Token Verification

`verify_refresh_token()` must:

- read `refresh_token:<refresh_token>`;
- return false if the key does not exist;
- parse the JSON metadata;
- reject revoked tokens;
- reject wrong device id;
- preserve current wrong-device behavior: wrong device attempts revoke the
  refresh token;
- populate `UserTokenInfo` on success.

### Refresh Token Revocation

`revoke_refresh_token()` must:

- mark the per-token JSON metadata as `revoked=true`;
- preserve the key TTL if feasible;
- return false if the token key does not exist.

`unrevoke_refresh_token()` must:

- mark the per-token JSON metadata as `revoked=false`;
- preserve the key TTL if feasible;
- return false if the token key does not exist.

`del_refresh_token()` must:

- delete `refresh_token:<refresh_token>`;
- remove the token from `user:<user_id>:refresh_tokens` when user metadata is
  still available;
- return true when deletion succeeds or the token is already absent.

### Refresh Token Rotation

`should_rotate_refresh_token()` must read the per-token metadata key.

Do not redesign rotation policy in this task. Keep existing threshold semantics
unless tests expose a correctness bug.

### Access Token Revocation

Replace the shared `revoked_access_tokens` set with per-JTI keys:

```text
revoked_access_token:<jti>
```

`revoke_token()` must:

- extract the JWT `jti`;
- set the per-JTI key;
- apply TTL aligned with the token's remaining lifetime when the token has a
  valid expiry;
- return false for invalid tokens or missing `jti`.

`is_token_revoked()` must:

- return true if `revoked_access_token:<jti>` exists;
- keep the existing conservative behavior of treating Redis errors as revoked.

`unrevoke_token()` must delete the per-JTI key.

## Redis Wrapper Requirements

If the current Redis wrapper lacks primitives needed for this task, add the
smallest reasonable methods. Likely useful additions:

- `set(key, value)`
- `get(key) -> optional<string>`
- `exists(key) -> bool`
- optionally `ttl(key) -> int64_t` if preserving TTL requires it

Keep the wrapper hiredis-based. Do not reintroduce `redis-plus-plus`.

Add focused Redis wrapper tests only if new primitives are added.

## Test Requirements

Update `test/auth/test_auth_tokens.cpp` to verify:

1. Generated refresh token creates a per-token key.
2. `refresh_tokens` shared hash is no longer used for new tokens.
3. Two refresh tokens with different explicit expiries do not invalidate each
   other.
4. Wrong-device refresh verification revokes only that token.
5. Deleted refresh token cannot be verified.
6. Revoked access token creates a per-JTI key and verification rejects it.
7. Unrevoked access token deletes the per-JTI key and verification accepts it
   again.

Keep tests deterministic. It is acceptable to use short sleeps only where
testing TTL behavior requires it, but prefer direct Redis existence checks over
slow sleeps.

## Required Verification

Start dependencies:

```bash
docker compose up -d redis postgres
```

Build focused targets:

```bash
cmake -S . -B /tmp/mychat-task1 \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task1 --target test_auth_tokens test_redis_hiredis gateway_server -j2
```

Run focused tests:

```bash
ctest --test-dir /tmp/mychat-task1 -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Gateway smoke:

```bash
/tmp/mychat-task1/gateway/gateway_server --config config/dev.json
curl http://127.0.0.1:8102/api/v1/health
```

Stop the Gateway with SIGINT after the smoke check and record the exit code.

## Documentation Requirements

Update:

- `docs/devlog/current_progress.md`
- `docs/devlog/phase1_gateway_auth_baseline.md`

Mention:

- new Redis key layout;
- tests run;
- remaining limitations, if any.

## Out Of Scope

- User Service implementation.
- PostgreSQL/ODB build integration.
- Gateway HTTP auth endpoints.
- gRPC service regeneration.
- Redis connection pool redesign.
- Broad formatting or unrelated warning cleanup.

## Acceptance Criteria

The task is ready for review when:

- code implements the new per-token/per-JTI Redis model;
- focused Auth tests cover the new model;
- Redis wrapper tests pass if wrapper primitives were added;
- Gateway still builds and health smoke passes;
- docs are updated;
- `docs/agent_context/task1-summary.md` is written using the required format.
