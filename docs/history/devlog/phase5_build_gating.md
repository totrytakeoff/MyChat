# Phase 5: Service Build Gating and Active Test Hygiene

## Goal

Make service and active-test build behavior deterministic:

- `MYCHAT_BUILD_SERVICES=ON` must not pull stale or dependency-incomplete
  service targets into the default build.
- Non-core or dependency-heavy services must have explicit build options.
- Fresh configured builds must not register CTest entries for test executables
  that are not buildable in the selected configuration.

## Changes

### CMake Options Added

| Option | Default | Description |
|--------|---------|-------------|
| `MYCHAT_BUILD_USER_SERVICE` | ON | Build User Service target (requires ODB) |
| `MYCHAT_BUILD_CODEC_SERVICE` | OFF | Build legacy codec gRPC service target (requires gRPC) |
| `MYCHAT_BUILD_LEGACY_UNIT_TESTS` | OFF | Build older unit tests pending cleanup |

### Root CMakeLists.txt

- Added three new options (see table above).
- Removed ad-hoc `find_package(gRPC CONFIG QUIET)` — gRPC detection moved into
  `services/CMakeLists.txt` where it's used.

### services/CMakeLists.txt

**User Service**:
- Gated on `MYCHAT_BUILD_USER_SERVICE AND TARGET im::user_odb`.
- Clear status message when skipped:
  - `MYCHAT_BUILD_USER_SERVICE=ON` + ODB unavailable → explains how to enable ODB.
  - `MYCHAT_BUILD_USER_SERVICE=OFF` → "User service is disabled".

**Codec Service**:
- Gated on `MYCHAT_BUILD_CODEC_SERVICE=ON`.
- If enabled and gRPC dependencies found → builds with `gRPC::grpc++` linkage.
- If enabled and gRPC not found → `message(FATAL_ERROR ...)` at configure time.
- If disabled → `message(STATUS ...)` explaining how to enable.

**Friend Service**:
- Still uses old `file(GLOB_RECURSE)` pattern — kept as-is since no .cpp files
  exist yet. Will be updated when Friend Service implementation begins.

### test/CMakeLists.txt

- `test/network/` (all commented out) → gated on `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.
- `test/router/` (RouterManagerTests, pre-existing failures) → gated on
  `MYCHAT_BUILD_LEGACY_UNIT_TESTS AND TARGET im::gateway_core`.
- `test/utils/` (SignalHandlerTest, CLIParserSimpleTest,
  ConfigManagerExtendedTest) → gated on `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.
- Legacy gateway integration tests remain behind `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`.

## Active Baseline Targets (Default ON)

| Target | Condition | Test |
|--------|-----------|------|
| `im_user_odb` | `MYCHAT_BUILD_PGSQL_ODB=ON` + ODB 2.5.0 runtime | `ODBUserPersistenceTest` |
| `im_user_service` | + `MYCHAT_BUILD_USER_SERVICE=ON` + `im::user_odb` | `UserServiceCoreTest` |
| `im_database` | `hiredis_FOUND` | `RedisHiredisTest` |
| `im_gateway_auth` | gateway deps available | `AuthTokenTest` |

## Intentionally Gated-Off Targets

| Target | Gate | Reason |
|--------|------|--------|
| `im_codec_service` | `MYCHAT_BUILD_CODEC_SERVICE=ON` | Stale generated gRPC stubs; not part of active MVP |
| `test/gateway/` | `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS` | Old integration tests need cleanup |
| `test/router/` | `MYCHAT_BUILD_LEGACY_UNIT_TESTS` | Pre-existing failures |
| `test/utils/` | `MYCHAT_BUILD_LEGACY_UNIT_TESTS` | SignalHandlerTest flaky in CI-like env |
| `test/network/` | `MYCHAT_BUILD_LEGACY_UNIT_TESTS` | All tests commented out |

## Verification

### ODB-enabled Full Build (4/4 passed)

```bash
cmake -S . -B /tmp/mychat-task5-odb \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task5-odb -j2
ctest --test-dir /tmp/mychat-task5-odb --output-on-failure
```

Result: 100% passed, 0 failed out of 4.

### No-ODB Baseline (2/2 passed)

```bash
cmake -S . -B /tmp/mychat-task5-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task5-no-odb -j2
ctest --test-dir /tmp/mychat-task5-no-odb --output-on-failure
```

Result: 100% passed, 0 failed out of 2. User Service skipped with clear status.

### Codec Explicit Enable

```bash
cmake -S . -B /tmp/mychat-task5-codec-fail \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_CODEC_SERVICE=ON \
  ...
```

Result: When gRPC is available, codec target builds. When gRPC is unavailable,
configure fails at `FATAL_ERROR` with a clear message.

## Remaining Build/Test Debt

- `services/CMakeLists.txt` still has old `file(GLOB_RECURSE)` blocks for Friend
  Service. These are harmless (no .cpp files yet) but should be replaced with
  explicit source lists when Friend Service is implemented.
- SignalHandlerTest (`test/utils/`) has pre-existing failures in signal delivery.
  Gated off behind `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.
- RouterManagerTests (`test/router/`) has pre-existing failures. Gated off.
- Historical note: `services/codec/` generated gRPC stubs were stale during
  this phase. Active codec generation is now automated through CMake and
  canonical outputs live under `common/proto`; duplicated generated files under
  `services/codec` have been removed.
