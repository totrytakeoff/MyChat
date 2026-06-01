# Task 5 Plan: Service Build Gating and Active Test Hygiene

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 4 completed and was approved:

- `im_user_service` now exists as the User Service core target.
- User register/login/profile behavior is backed by ODB/PostgreSQL.
- `UserServiceCoreTest`, `ODBUserPersistenceTest`, `AuthTokenTest`, and
  `RedisHiredisTest` pass against Docker dependencies.
- Commit: `569be0c Add user service core baseline`.

During review, a broader build hygiene problem was confirmed:

```text
cmake --build /tmp/mychat-task4-review-user2 -j2
```

fails because `services/CMakeLists.txt` unconditionally builds
`im_codec_service`, whose generated gRPC source includes:

```text
grpcpp/generic/async_generic_service.h
```

The current dependency set does not provide that header. This issue predates
Task 4, but it blocks a clean "build all configured targets" workflow when
`MYCHAT_BUILD_SERVICES=ON`.

Before Phase E Gateway-to-User integration, the service build switches must be
made explicit and predictable.

## Goal

Make service and active-test build behavior deterministic:

1. `MYCHAT_BUILD_SERVICES=ON` must not pull stale or dependency-incomplete
   service targets into the default build.
2. `im_user_service` must remain enabled when ODB is enabled and must continue
   to build/test successfully.
3. Codec/gRPC service code must be behind an explicit gate. It should either:
   - stay disabled by default with a clear status message; or
   - fail early with a clear configure-time message only when explicitly
     requested and dependencies/files are missing.
4. Fresh configured builds must not register active CTest entries for test
   executables that are not buildable in the selected configuration.
5. Documentation must record the new build switches and verification commands.

This task is build-system cleanup, not Gateway feature development.

## Relevant Files

Read before editing:

- `CMakeLists.txt`
- `services/CMakeLists.txt`
- `services/user/CMakeLists.txt`
- `test/CMakeLists.txt`
- `test/router/CMakeLists.txt`
- `test/utils/CMakeLists.txt`
- `docs/devlog/current_progress.md`
- `docs/architecture/service_mvp_roadmap.md`
- `docs/agent_context/task4-review.md`

Likely files to update:

- `CMakeLists.txt`
- `services/CMakeLists.txt`
- `test/CMakeLists.txt`
- `test/router/CMakeLists.txt`
- `test/utils/CMakeLists.txt`
- `docs/devlog/current_progress.md`
- `docs/devlog/phase5_build_gating.md`
- `docs/architecture/service_mvp_roadmap.md`
- `docs/agent_context/task5-summary.md`

## Required Design

### Service Build Options

Add explicit build options in the root CMake file or a clearly scoped CMake
module. Suggested names:

```cmake
option(MYCHAT_BUILD_USER_SERVICE "Build User Service target" ON)
option(MYCHAT_BUILD_CODEC_SERVICE "Build legacy codec gRPC service target" OFF)
```

Rules:

- `MYCHAT_BUILD_SERVICES=OFF` disables all service targets.
- `MYCHAT_BUILD_SERVICES=ON` allows service targets, but each non-core or
  dependency-heavy service must still respect its own option.
- `MYCHAT_BUILD_USER_SERVICE=ON` should add `services/user` only when
  `TARGET im::user_odb` exists. If ODB is unavailable, print a clear status
  message and skip User Service.
- `MYCHAT_BUILD_CODEC_SERVICE=OFF` should skip `im_codec_service` by default.
- `MYCHAT_BUILD_CODEC_SERVICE=ON` must verify required generated files and gRPC
  dependencies at configure time. If they are missing, fail with a concise
  actionable message.

Do not silently add a new vcpkg feature or regenerate proto/gRPC artifacts in
this task. Codec regeneration should be its own later task.

### Test Hygiene

The current test tree contains a mix of active baseline tests and older tests.
Make the selected configuration explicit:

- Active baseline tests:
  - `RedisHiredisTest`
  - `AuthTokenTest`
  - `ODBUserPersistenceTest` when ODB is enabled
  - `UserServiceCoreTest` when User Service is enabled
- Legacy or cleanup-needed tests should be gated behind explicit options, for
  example:

```cmake
option(MYCHAT_BUILD_LEGACY_UNIT_TESTS "Build older unit tests pending cleanup" OFF)
```

or more specific names if cleaner.

Expected behavior:

- Fresh `ctest` must not report "Could not find executable" for skipped tests.
- If a test is registered with `add_test`, its target must be built by the
  normal `cmake --build <build-dir>` default target in that configuration.
- Avoid leaving unconditional `add_subdirectory(...)` calls for test groups
  whose targets are known stale or out of the active MVP baseline.

### Documentation

Create `docs/devlog/phase5_build_gating.md` with:

- What options were added or changed.
- Which targets are active by default.
- Which targets are intentionally gated off.
- Exact verification commands and results.
- Remaining known build/test debt.

Update:

- `docs/devlog/current_progress.md`
- `docs/architecture/service_mvp_roadmap.md`

## Acceptance Criteria

Use Docker dependencies:

```bash
docker compose up -d redis postgres
```

### Fresh ODB-enabled Full Build

From a clean build directory:

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

Expected:

- Configure succeeds.
- Default build succeeds.
- CTest runs only active buildable tests and passes.
- `im_codec_service` is not built unless `MYCHAT_BUILD_CODEC_SERVICE=ON`.
- `im_user_service` and `test_user_service` are built.

### Fresh No-ODB Baseline

From a clean build directory:

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

Expected:

- Configure succeeds.
- Default build succeeds.
- User Service and ODB tests are skipped with clear status messages.
- Gateway/Auth/Redis active baseline tests still pass.

### Explicit Codec Failure Path

Run a configure with:

```bash
-DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_CODEC_SERVICE=ON
```

Expected:

- If gRPC dependencies are unavailable, configure fails early with a clear
  message explaining what is missing.
- If dependencies are available, the codec target may build, but do not expand
  this task into codec regeneration.

## Non-Goals

- Do not implement Gateway-to-User routes in this task.
- Do not change User Service business behavior unless required to keep builds
  passing.
- Do not regenerate protobuf/gRPC service artifacts.
- Do not repair `PgSqlConnection` unless it unexpectedly blocks the build.
- Do not enable old gateway integration tests by default.

## Review Notes For Implementation Agent

- Keep the CMake surface boring and explicit. This project already suffered
  from stale generated code being pulled into unrelated work.
- Prefer clear `message(STATUS ...)` for skipped optional targets and
  `message(FATAL_ERROR ...)` only when a developer explicitly requested a target
  that cannot be built.
- If any old test fails during full `ctest`, decide whether it belongs to the
  active MVP baseline. If not, gate it off and document why. If yes, fix it with
  focused changes and tests.
