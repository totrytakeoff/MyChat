# Phase 1 Gateway/Auth Baseline

## Purpose

Phase 1 turns the recovered build foundation into a small runnable baseline:
Docker services are healthy, the gateway starts from one config file, Redis has
focused tests, and the existing JWT auth code has a focused token test suite.

## Architecture Decisions

- The active gateway implementation now uses the canonical file name
  `gateway/gateway_server/gateway_server.cpp`.
- The temporary duplicate `gateway_server-t1.cpp` was removed from the source
  tree and from CMake.
- Older gateway integration tests are disabled by default behind
  `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`. They still reflect older dependencies and
  should be repaired incrementally instead of blocking the current baseline.
- Auth token tests live under `test/auth`, next to other top-level test modules.

## Fixes Made

- Removed the stale duplicate gateway implementation.
- Fixed `GatewayServer` member initialization order warnings.
- Changed header helper functions in `multi_platform_auth.hpp` from `static` to
  `inline` to avoid unused internal-linkage function warnings in translation
  units that include the header.
- Added focused Auth token tests covering:
  - access token generation and verification
  - access token expiry rejection
  - refresh token verification and wrong-device revocation
  - access token refresh from refresh token
  - access token revoke/unrevoke
  - refresh token deletion
- Fixed `generate_refresh_token()` so Redis TTL uses the actual requested expiry
  seconds. The old code used the platform default TTL even when callers passed
  an explicit `expire_seconds`; when config loading failed this could expire the
  whole `refresh_tokens` hash immediately.
- Made Auth tests use an absolute source-tree config path instead of depending
  on the current working directory used by CTest.
- **Task 1**: Auth Redis token storage hardened to per-token/per-JTI model:
  - `refresh_token:<token>` per-token metadata keys with independent TTL.
  - `revoked_access_token:<jti>` per-JTI keys with TTL aligned to token expiry.
  - `user:<user_id>:refresh_tokens` user index set.
  - Added `set`, `get`, `exists`, `ttl`, `keys` primitives to RedisClient.
  - Added test coverage: per-token key existence, independent TTLs, wrong-device
    isolation, per-JTI key lifecycle.

## Verification

Docker services:

```bash
docker compose up -d redis postgres
docker compose ps
```

Both services were healthy:

- `mychat-redis`
- `mychat-postgres`

Builds:

```bash
cmake --build /tmp/mychat-phase1 --target gateway_server -j2
cmake --build /tmp/mychat-phase1-tests --target test_auth_tokens test_redis_hiredis -j2
```

Focused tests:

```bash
ctest --test-dir /tmp/mychat-phase1-tests -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 2
```

Gateway smoke test:

```bash
/tmp/mychat-phase1/gateway/gateway_server --config config/dev.json
curl http://127.0.0.1:8102/api/v1/health
```

The health endpoint returned:

```json
{"status": "ok"}
```

SIGINT shutdown exited with code `0`.

## Auth Redis Model (Post-Task-1)

Refresh tokens now use per-token keys:

```text
refresh_token:<token>       # JSON metadata, independent TTL
user:<user_id>:refresh_tokens  # user index set (SADD/SREM)
```

Access token revocation uses per-JTI keys:

```text
revoked_access_token:<jti>  # TTL aligned to remaining token lifetime
```

Redis primitives added: `set`, `get`, `exists`, `ttl`, `keys`.

## Current Auth Caveats

- Redis wrapper is single-connection and mutex-serialized. Adequate for
  correctness, not for performance.
- Wrong-device refresh attempts revoke the refresh token. This behavior is
  documented by the test suite.

## Next Work

1. Verify and document the ODB compiler/toolchain, then bring the PostgreSQL ODB
   wrapper into the validated build.
2. Build `services/user` as an independent User Service MVP using ODB-backed
   PostgreSQL persistence.
3. Wire Gateway auth routes to the User Service contract:
   `/api/v1/auth/register`, `/api/v1/auth/login`, and `/api/v1/auth/refresh`.
4. Add focused service-level tests before enabling endpoint-level Gateway/User
   integration tests.
