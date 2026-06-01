# Task 6 Plan: Gateway to User Service HTTP Integration

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 5 completed the build hygiene gate:

- `MYCHAT_BUILD_SERVICES=ON` no longer pulls stale codec/gRPC targets by
  default.
- `im_user_service` builds when ODB is enabled.
- Fresh default builds and CTest pass in both ODB and no-ODB configurations.
- Commit: `632018c Gate service builds and active tests`.

The project can now start Phase E:

```text
Gateway HTTP routes -> User Service core -> ODB/PostgreSQL
                     -> Gateway Auth token generation -> Redis
```

This task should integrate the already-built User Service core into Gateway for
the first usable auth/account flow. Keep the scope narrow: HTTP register,
login, and profile/info. Do not attempt WebSocket auth redesign, gRPC
generation, Message Service, or full API polish in this task.

## Goal

Create the first end-to-end Gateway/User vertical slice:

1. Gateway can handle HTTP registration through User Service.
2. Gateway can handle HTTP login through User Service and issue access/refresh
   tokens through `MultiPlatformAuthManager`.
3. Gateway can return the current user's profile/info using a valid Bearer
   access token.
4. Duplicate account, wrong password, missing account/password, missing or
   invalid token return stable JSON errors and appropriate HTTP status codes.
5. Tests prove the flow against Docker PostgreSQL and Redis.
6. Existing ODB/no-ODB build gates remain green.

## Relevant Files

Read before editing:

- `gateway/gateway_server/gateway_server.hpp`
- `gateway/gateway_server/gateway_server.cpp`
- `gateway/CMakeLists.txt`
- `gateway/auth/multi_platform_auth.hpp`
- `gateway/auth/multi_platform_auth.cpp`
- `services/user/user_service.hpp`
- `services/user/user_service.cpp`
- `services/user/user_repository.hpp`
- `services/user/password_hasher.hpp`
- `config/dev.json`
- `common/utils/http_utils.hpp`
- `test/auth/test_auth_tokens.cpp`
- `test/user/test_user_service.cpp`
- `test/CMakeLists.txt`
- `docs/devlog/current_progress.md`
- `docs/architecture/service_mvp_roadmap.md`
- `docs/agent_context/task5-review.md`

Likely files to create/update:

- `gateway/user_http_controller.hpp`
- `gateway/user_http_controller.cpp`
- `gateway/gateway_server/gateway_server.hpp`
- `gateway/gateway_server/gateway_server.cpp`
- `gateway/CMakeLists.txt`
- `config/dev.json`
- `test/gateway_user/CMakeLists.txt`
- `test/gateway_user/test_gateway_user_http.cpp`
- `test/CMakeLists.txt`
- `docs/devlog/phase6_gateway_user_integration.md`
- `docs/devlog/current_progress.md`
- `docs/architecture/service_mvp_roadmap.md`
- `docs/agent_context/task6-summary.md`

## Required Design

### Controller Boundary

Do not bury all JSON parsing and auth flow directly inside
`GatewayServer::init_http_server`.

Create a small Gateway-side controller, for example:

```cpp
namespace im::gateway {

class UserHttpController {
public:
    UserHttpController(std::shared_ptr<im::service::user::UserService> users,
                       std::shared_ptr<MultiPlatformAuthManager> auth);

    void register_user(const httplib::Request& req, httplib::Response& res);
    void login(const httplib::Request& req, httplib::Response& res);
    void current_user(const httplib::Request& req, httplib::Response& res);
};

}
```

The controller should be independently testable without starting the whole
Gateway server if practical. If an end-to-end HTTP server test is simpler, keep
ports isolated and deterministic.

### Gateway Routes

Use the existing API prefix from `config/dev.json`:

- `POST /api/v1/auth/register`
- `POST /api/v1/auth/login`
- `GET /api/v1/auth/info`

Update `config/dev.json` route metadata if needed, but do not rely on the old
generic message parser path for these three MVP endpoints. It is acceptable to
register these as direct `httplib` handlers before the catch-all route in
`GatewayServer::init_http_server`.

Keep the old catch-all `Get(".*")` / `Post(".*")` path for non-MVP routes.

### User Service Construction

Gateway must only create/use User Service when the target is available:

- If `im::user_service` exists, link Gateway integration to it.
- If ODB/User Service is not enabled, Gateway should still build for the
  no-ODB baseline, but user HTTP endpoints may return a clear `503` or may not
  be registered. Pick one behavior and document it.

Prefer an explicit CMake option if needed:

```cmake
option(MYCHAT_ENABLE_GATEWAY_USER_HTTP "Enable Gateway HTTP routes backed by User Service" ON)
```

Rules:

- ODB-enabled build: routes are active.
- No-ODB build: Gateway/Auth/Redis baseline remains green.
- Do not make no-ODB builds fail just because User Service is unavailable.

### Database Configuration

Tests currently use:

```text
host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass
```

For this task, a simple config-derived or helper-built connection string is
acceptable, but avoid scattering hard-coded DB strings through production code.

Recommended:

- Add a small internal helper that reads PostgreSQL settings from `config/dev.json`
  if already present.
- If config lacks those fields, add a minimal `postgres` section to
  `config/dev.json`.
- Tests may still use explicit test constants if isolated and documented.

### JSON Contract

Use `nlohmann::json`.

Request examples:

```json
POST /api/v1/auth/register
{
  "account": "alice",
  "password": "pass123",
  "nickname": "Alice"
}
```

```json
POST /api/v1/auth/login
{
  "account": "alice",
  "password": "pass123",
  "device_id": "web-1",
  "platform": "web"
}
```

```text
GET /api/v1/auth/info
Authorization: Bearer <access_token>
```

Response shape should be stable and simple:

```json
{
  "code": 0,
  "message": "ok",
  "data": {}
}
```

Use non-zero `code` for errors. Do not expose `password_hash`.

Suggested HTTP statuses:

- `200` success
- `400` malformed JSON / missing required field
- `401` wrong password / invalid token
- `404` token valid but profile missing
- `409` duplicate account
- `503` User Service unavailable in no-ODB builds

### Token Behavior

On successful login:

- Authenticate password through `UserService::login_by_account`.
- Generate access and refresh tokens using `MultiPlatformAuthManager`.
- Use `profile.uid` as `user_id`.
- Use `profile.account` as username/account.
- Use request `device_id` and `platform`, defaulting to `"default-device"` and
  `"web"` only if omitted.
- Return tokens and profile in JSON.

On `/auth/info`:

- Extract `Authorization: Bearer ...`.
- Verify access token through `MultiPlatformAuthManager`.
- Fetch profile by token user id.
- Return profile without token internals and without password hash.

## Required Tests

Add focused tests under `test/gateway_user`.

Minimum cases:

1. Register succeeds and returns a profile without `password_hash`.
2. Duplicate register returns conflict.
3. Login succeeds and returns access/refresh tokens plus profile.
4. Login wrong password returns unauthorized.
5. `/auth/info` with valid Bearer token returns the profile.
6. `/auth/info` with missing/invalid token returns unauthorized.

Tests should:

- Use Docker PostgreSQL and Redis.
- Clean only their own data:
  - PostgreSQL accounts with prefix `task6-test-%`.
  - Redis keys created by token tests.
- Avoid `DROP TABLE`.
- Avoid relying on random fixed ports if possible. If an HTTP server test needs
  ports, use a clearly isolated high port and stop the server reliably.

## Acceptance Criteria

Use Docker dependencies:

```bash
docker compose up -d redis postgres
```

### ODB-enabled Build and Tests

```bash
cmake -S . -B /tmp/mychat-task6-odb \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task6-odb -j2
ctest --test-dir /tmp/mychat-task6-odb --output-on-failure
```

Expected:

- Default build succeeds.
- Existing active tests still pass.
- New Gateway/User HTTP tests pass.

### No-ODB Baseline

```bash
cmake -S . -B /tmp/mychat-task6-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task6-no-odb -j2
ctest --test-dir /tmp/mychat-task6-no-odb --output-on-failure
```

Expected:

- Default build succeeds.
- User Service-backed Gateway tests are skipped or not registered with a clear
  status message.
- Redis/Auth baseline tests pass.

## Non-Goals

- Do not implement Message Service.
- Do not implement Friend/Group/Push service routes.
- Do not redesign WebSocket authentication.
- Do not regenerate gRPC/protobuf service stubs.
- Do not build a full API framework; keep the controller small.
- Do not repair unrelated legacy tests unless they block active baseline
  verification.

## Review Notes For Implementation Agent

- Keep build gates explicit. No-ODB builds must remain useful.
- Prefer a controller/helper that can be unit-tested over growing
  `GatewayServer::init_http_server`.
- Keep JSON responses stable and documented in the phase devlog.
- Use current project patterns (`httplib`, `nlohmann::json`,
  `MultiPlatformAuthManager`, `UserService`) before introducing new libraries.
