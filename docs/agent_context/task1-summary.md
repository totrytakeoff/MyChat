# Task 1 Summary

## Files Changed

- `common/database/redis/redis_mgr.hpp` — Added `set()`, `get()`, `exists()`, `ttl()`, `keys()` declarations to `RedisClient`.
- `common/database/redis/redis_mgr.cpp` — Implemented `set()`, `get()`, `exists()`, `ttl()`, `keys()` for hiredis-backed RedisClient.
- `gateway/auth/multi_platform_auth.cpp` — Rewrote all Redis key access:
  - `generate_refresh_token()`: writes `refresh_token:<rt>` per-token key instead of `refresh_tokens` hash; user index changed from `user:<uid>:rt` to `user:<uid>:refresh_tokens`.
  - `verify_refresh_token()`: reads `refresh_token:<rt>` per-token key.
  - `revoke_refresh_token()`: reads/rewrites per-token key with TTL preservation; returns false if key missing.
  - `unrevoke_refresh_token()`: same pattern.
  - `del_refresh_token()`: deletes per-token key; removes from user index when metadata available.
  - `should_rotate_refresh_token()`: reads per-token key.
  - `revoke_token()`: sets `revoked_access_token:<jti>` per-JTI key with TTL aligned to token remaining lifetime.
  - `unrevoke_token()`: deletes per-JTI key.
  - `is_token_revoked()`: uses `exists()` on per-JTI key instead of `sismember()` on shared set.
- `test/auth/test_auth_tokens.cpp` — Updated cleanup to clear new key patterns; added 4 new tests:
  - `RefreshTokenCreatesPerTokenKey`: verifies per-token key exists and old hash is not used.
  - `IndependentExpiryPerRefreshToken`: two tokens with different TTLs do not interfere.
  - `WrongDeviceRevokesOnlyThatToken`: wrong-device attempt on rt1 does not affect rt2 for the same user.
  - `RevokedAccessTokenCreatesPerJtiKey`: per-JTI key created on revoke, deleted on unrevoke.
- `docs/devlog/current_progress.md` — Moved Auth token storage from "In Progress" to "Completed Work"; updated verified commands and results.
- `docs/devlog/phase1_gateway_auth_baseline.md` — Replaced caveats section with new Redis key layout description; added Task 1 entry to Fixes Made.

## Behavior Changed

- Refresh tokens no longer share a single Redis hash. Each `refresh_token:<token>` key has its own TTL. Expiring one token cannot affect another.
- Access token revocation no longer appends to a shared set. Each `revoked_access_token:<jti>` key has a TTL aligned to the token's remaining lifetime. Revoked tokens auto-expire.
- `revoke_refresh_token()` and `unrevoke_refresh_token()` now return `false` when the token key does not exist (previously returned `true` unconditionally).
- `del_refresh_token()` now also removes the token from the user index (`user:<uid>:refresh_tokens`) when metadata is available.
- User index renamed from `user:<uid>:rt` to `user:<uid>:refresh_tokens`.

## Verification

- Command: `cmake -S . -B /tmp/mychat-task1 -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON -DMYCHAT_BUILD_SERVICES=OFF -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF -DCMAKE_BUILD_TYPE=Debug`
  Result: Configuring done.
- Command: `cmake --build /tmp/mychat-task1 --target test_auth_tokens test_redis_hiredis gateway_server -j2`
  Result: Built all targets successfully (pre-existing warnings only, unrelated to this change).
- Command: `ctest --test-dir /tmp/mychat-task1 -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure`
  Result: 100% tests passed, 0 failed out of 2.
- Command: `curl http://127.0.0.1:8102/api/v1/health`
  Result: `{"status": "ok"}` — Gateway health smoke passed; SIGINT exit code 0.

## Deviations From Plan

- Added `keys()` method to RedisClient for test cleanup. The plan mentioned adding `set`, `get`, `exists`, and optionally `ttl`. `keys()` was added to support pattern-based cleanup in test teardown without requiring a broad `FLUSHDB`. This is a small, focused addition that does not change production behavior.
- Used `keys()`-based pattern deletion in test cleanup instead of per-key tracking. This is more robust for isolating tests that generate unknown key names.

## Known Issues / Follow-Up

- Redis wrapper remains single-connection and mutex-serialized. Adequate for correctness tests; not for performance.
- Wrong-device refresh revocation behavior is preserved and tested.
- Next step per roadmap: ODB toolchain verification and PostgreSQL/ODB build integration.
