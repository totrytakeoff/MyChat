# Task 6 Summary: Gateway-to-User Service HTTP Integration

**Status**: ✅ Completed

## Objective

Integrate the User Service core (Task 4) into Gateway HTTP routes, enabling the first end-to-end register/login/profile vertical slice through REST API.

## Changes

### New Files

| File | Purpose |
|------|---------|
| `gateway/user_http_controller.hpp` | `UserHttpController` class — handles register, login, profile requests |
| `gateway/user_http_controller.cpp` | Implementation: JSON parsing, field validation, `UserService` calls, token issuance via `MultiPlatformAuthManager::generate_tokens`, HTTP status codes (201/200/400/401/409/500) |
| `test/gateway_user/CMakeLists.txt` | Test target linking `im::gateway_core`, `im::gateway_auth`, `im::user_service` |
| `test/gateway_user/test_gateway_user_http.cpp` | 10 tests: 9 direct controller tests + 1 full route-layer integration test via `register_user_http_routes_on_server` |

### Modified Files

| File | Change |
|------|--------|
| `gateway/gateway_server.hpp` | Forward declarations for `odb::pgsql::database` and `UserHttpController`; conditional `odb_db_` and `user_http_controller_` members behind `IM_ENABLE_USER_HTTP`; `register_user_http_routes()` method declaration |
| `gateway/gateway_server.cpp` | Conditionally includes ODB + UserService headers; defines free function `register_user_http_routes_on_server` (unit-testable seam); creates controller before HTTP server setup; registers user routes inside `init_http_server()` after health check and before legacy catch-all handlers |
| `gateway/CMakeLists.txt` | Conditional source/link/define when `TARGET im::user_service` exists |
| `test/CMakeLists.txt` | Adds `gateway_user` subdirectory when both `im::user_service` and `im::gateway_core` exist |
| `CMakeLists.txt` (root) | Moved `add_subdirectory(services)` before `add_subdirectory(gateway)` so CMake target order is correct |

## API Endpoints

| Method | Path | Auth | Returns |
|--------|------|------|---------|
| POST | `/api/v1/auth/register` | No | 201 + profile + tokens |
| POST | `/api/v1/auth/login` | No | 200 + profile + tokens |
| GET | `/api/v1/auth/info` | Bearer token | 200 + profile |

## Error Responses

- **400** — Missing required fields (account, password)
- **401** — Wrong password (`WRONG_PASSWORD`), unknown account (`INVALID_ACCOUNT`), invalid/missing token
- **404** — User profile not found (valid token but non-existent UID)
- **409** — Duplicate account on register
- **500** — Internal server error

## Route Registration Architecture

User HTTP routes are registered inside `init_http_server()` after the health
check and before the legacy catch-all handlers. This ordering is required
because httplib dispatches the first matching handler in registration order;
the catch-all `.*` routes would otherwise intercept `/api/v1/auth/*`.

1. The `UserHttpController` is created before `init_http_server()` runs.
2. `init_http_server()` registers `/api/v1/health`.
3. `init_http_server()` calls `register_user_http_routes()`, which delegates
   to `register_user_http_routes_on_server()`.
4. Legacy `Get(".*")` / `Post(".*")` handlers are registered last.

The free function is declared in `gateway_server.cpp` (global namespace) and
serves as a **unit-testable seam**. The route-layer test imports it directly
with a forward declaration, registers catch-all handlers after it, and asserts
that user routes are not swallowed by the catch-all.

## Build & Test Results

| Configuration | Tests | Result |
|---------------|-------|--------|
| ODB-enabled (`MYCHAT_BUILD_PGSQL_ODB=ON`) | 5/5 (RedisHiredis, ODBUserPersistence, UserServiceCore, **GatewayUserHttp** 10/10, AuthToken) | ✅ |
| No-ODB baseline | 2/2 (RedisHiredis, AuthToken) | ✅ |

## Pre-existing Issues Found

- `SPDLOG_FMT_STRING` consteval incompatibility with clang++ 16 — workaround: use `g++`
- CMake `add_subdirectory` ordering: `gateway` evaluated before `services`, so `TARGET im::user_service` failed — fixed by reordering in root `CMakeLists.txt`

## Review Closure

- Fixed route ordering: user routes are registered before catch-all handlers.
- Fixed `ProfileNonExistentUidReturns404`: the test now uses a valid token for
  a missing UID and expects `404`.
- Hardened route-layer test to include catch-all handlers after user routes.
- Added scoped server cleanup in the route-layer test so failed assertions do
  not leave a joinable server thread.
