# Phase 6: Gateway-to-User Service HTTP Integration

## Goal

Wire the User Service core (Phase 4) into Gateway HTTP routes for the first
end-to-end register/login/profile vertical slice.

## Files Changed

### Created

- `gateway/user_http_controller.hpp` — Controller class with `handle_register`,
  `handle_login`, `handle_profile` methods.
- `gateway/user_http_controller.cpp` — Implementation: JSON parsing, field
  validation, `UserService` calls, token issuance via
  `MultiPlatformAuthManager::generate_tokens`.
- `test/gateway_user/CMakeLists.txt` — Test target linking
  `im::gateway_core`, `im::gateway_auth`, `im::user_service`.
- `test/gateway_user/test_gateway_user_http.cpp` — 10 tests: 9 direct
  controller tests + 1 full HTTP route-layer integration test.

### Modified

- `gateway/gateway_server.hpp` — Forward declarations for `odb::pgsql::database`
  and `UserHttpController`; conditional members; `register_user_http_routes()`.
- `gateway/gateway_server.cpp` — Defines free function
  `register_user_http_routes_on_server()` as a unit-testable seam; creates
  controller before HTTP server setup; registers user routes after health check
  and before legacy catch-all handlers.
- `gateway/CMakeLists.txt` — Conditional source/link/define when
  `TARGET im::user_service`.
- `test/CMakeLists.txt` — Adds `gateway_user` when both `im::user_service` and
  `im::gateway_core` exist.
- `CMakeLists.txt` (root) — Moved `add_subdirectory(services)` before
  `add_subdirectory(gateway)` so `TARGET im::user_service` exists at CMake
  evaluation time.

## API Endpoints

| Method | Path | Auth | Status | Response |
|--------|------|------|--------|----------|
| POST | `/api/v1/auth/register` | none | 201 | `{profile, access_token, refresh_token}` |
| POST | `/api/v1/auth/login` | none | 200 | `{profile, access_token, refresh_token}` |
| GET | `/api/v1/auth/info` | Bearer | 200 | `{profile}` |

Error responses: 400 (missing fields), 401 (wrong password / invalid token),
404 (profile not found), 409 (duplicate account), 500 (internal error).

## Route Registration Architecture

User routes are registered **after** the controller exists and **before** the
legacy catch-all handlers. Both conditions matter:

1. `init_server()` creates `UserHttpController` before `init_http_server()`.
2. `init_http_server()` registers `/api/v1/health`.
3. `init_http_server()` calls `register_user_http_routes()`, delegating to the
   free function `register_user_http_routes_on_server()`.
4. Legacy `Get(".*")` / `Post(".*")` handlers are registered last.

httplib dispatches the first matching handler in registration order, so placing
`/api/v1/auth/*` after `.*` would make those endpoints unreachable.

The free function is declared in the global namespace and forward-declared in
the test, serving as a **unit-testable seam** for the route-layer integration
test. The test also registers catch-all handlers after the user routes to catch
future route-order regressions.

## Build & Test

| Configuration | Tests | Status |
|---------------|-------|--------|
| ODB-enabled | 5 suites, 10 GatewayUserHttp subtests | ✅ |
| No-ODB baseline | 2 suites | ✅ |

ODB-enabled build command:
```bash
cmake -S . -B build \
  -DCMAKE_CXX_COMPILER=g++ \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_USER_SERVICE=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Final reviewer verification used fresh `/tmp` build directories:

```bash
cmake -S . -B /tmp/mychat-task6-review-odb \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task6-review-odb -j2
ctest --test-dir /tmp/mychat-task6-review-odb --output-on-failure
```

Result: 5/5 tests passed.

```bash
cmake -S . -B /tmp/mychat-task6-review-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task6-review-no-odb -j2
ctest --test-dir /tmp/mychat-task6-review-no-odb --output-on-failure
```

Result: 2/2 tests passed.
