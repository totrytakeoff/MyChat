# Phase 16: CI/CD Engineering Baseline

Date: 2026-06-05

## Purpose

Promote the current local regression commands into reusable CI entrypoints
without changing project runtime behavior. The first CI/CD slice is split into
a lightweight default path and a heavier remote Push ODB/gRPC path.

## Current Decision

GitHub hosted CI is paused as of 2026-06-06 to keep the current development
loop fast. The local CI scripts remain as manual regression entrypoints, but
the hosted workflow file was removed after the manual heavy job proved too slow
and noisy for this phase.

Reason:

- The push/default hosted path was made green, but the manual
  `remote-push-odb` hosted job took 1h15m before failing.
- The heavy hosted job first exposed runner/toolchain setup issues
  (`bdep`/`build2` packages unavailable in Ubuntu 24.04 apt, then build2
  download URL, then first-run `.odb` path creation).
- After those environment issues were fixed, the hosted heavy job failed in a
  Gateway/Auth link stage. Continuing to chase hosted CI now would slow feature
  development more than it helps.
- Current project strategy is to preserve local regression scripts and revisit
  hosted CI near project stabilization/release hardening.

## Implemented State

New local CI scripts:

- `scripts/ci/checks.sh`
  - Validates `docs/agent_context/state.json` as JSON.
  - Runs whitespace checks for the latest commit and current working-tree diff.
- `scripts/ci/default_regression.sh`
  - Configures `build/ci-default`.
  - Uses `vcpkg_installed/default` by default so local and GitHub cache paths
    match without sharing installed packages with heavier feature builds.
  - Starts the Redis test dependency through Docker Compose when Docker is
    available; set `MYCHAT_CI_START_REDIS=OFF` to use an already-running Redis.
  - Removes stale checked-in protobuf outputs and regenerates them with the
    configured vcpkg toolchain before compiling.
  - Keeps default no-ODB/no-gRPC behavior.
  - Uses `MYCHAT_BUILD_SERVICES=OFF` for the default regression slice.
  - Builds and runs all tests registered in that build.
- `scripts/ci/remote_push_odb.sh`
  - Configures `build/ci-remote-push-odb`.
  - Uses `vcpkg_installed/remote-push-odb` by default so the heavier manifest
    feature install can be cached without causing default CI dependency churn.
  - Removes stale checked-in protobuf/gRPC outputs and regenerates them with
    the configured vcpkg toolchain before compiling.
  - Enables `pgsql-odb` and `codec-grpc` manifest features.
  - Enables services, Gateway, Push gRPC, and PostgreSQL/ODB.
  - Starts Redis/PostgreSQL through `docker compose` when Docker is available.
  - Applies PostgreSQL schema migrations through
    `scripts/db/migrate_postgres.sh` before configuring/building the ODB/gRPC
    regression.
  - Requires ODB 2.5.0 runtime at `.odb/installed` by default.
  - Can build the ODB runtime first when
    `MYCHAT_CI_BUILD_ODB_RUNTIME=ON`.
  - Builds `test_remote_push_gateway_server_smoke`, runs the process-level
    remote Push smoke, builds the full test set, then runs Push-focused tests
    and the full suite.

Hosted workflow state:

- `.github/workflows/ci.yml` was removed on 2026-06-06.
- Keep `scripts/ci/checks.sh`, `scripts/ci/default_regression.sh`, and
  `scripts/ci/remote_push_odb.sh` as local/manual entrypoints.
- Do not reintroduce hosted CI until the service/process boundaries and
  Gateway/Auth link surface are stable enough that CI work does not block
  normal feature iteration.

## CI Contract

Default CI must stay compatible with repository defaults:

```text
MYCHAT_BUILD_PUSH_GRPC_SERVICE=OFF
MYCHAT_BUILD_CODEC_SERVICE=OFF
MYCHAT_BUILD_PGSQL_ODB=OFF
```

The heavier remote Push job is the only first-slice CI path that enables:

```text
VCPKG_MANIFEST_FEATURES=pgsql-odb;codec-grpc
MYCHAT_BUILD_SERVICES=ON
MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON
MYCHAT_BUILD_PGSQL_ODB=ON
```

Checked-in proto/gRPC outputs remain governed by CMake `generate_proto`.
The CI work did not introduce ad-hoc `protoc` generation.

Hosted runner compatibility fixes:

- CI regenerates protobuf/gRPC outputs with the configured vcpkg toolchain
  before compiling, because checked-in generated files may have been produced
  by a different protobuf version.
- The commit whitespace check fetches the previous commit and checks
  `HEAD^..HEAD` when a parent exists, so shallow checkout history does not
  make historical tree whitespace fail a new commit.
- Gateway authentication includes jwt-cpp's nlohmann-json defaults header
  instead of the raw `jwt-cpp/jwt.h` entrypoint, because newer jwt-cpp releases
  no longer let `jwt::verify()` and `jwt::decode()` infer JSON traits from
  zero-argument calls.

## Local Commands

Lightweight regression:

```bash
scripts/ci/checks.sh
scripts/ci/default_regression.sh
```

Remote Push ODB/gRPC regression:

```bash
scripts/build_odb_runtime_2_5.sh
scripts/ci/remote_push_odb.sh
```

If the ODB runtime should be built by the CI script:

```bash
MYCHAT_CI_BUILD_ODB_RUNTIME=ON scripts/ci/remote_push_odb.sh
```

Useful overrides:

```bash
MYCHAT_CI_JOBS=4 scripts/ci/default_regression.sh
MYCHAT_CI_DEFAULT_BUILD_DIR=build/default-regression scripts/ci/default_regression.sh
MYCHAT_CI_REMOTE_PUSH_BUILD_DIR=build/remote-push-odb scripts/ci/remote_push_odb.sh
MYCHAT_CI_VCPKG_INSTALLED_DIR=/path/to/vcpkg_installed_slice scripts/ci/default_regression.sh
MYCHAT_CI_START_REDIS=OFF scripts/ci/default_regression.sh
MYCHAT_ODB_ROOT=/path/to/odb-2.5.0 scripts/ci/remote_push_odb.sh
```

## Remaining Work

- Revisit GitHub hosted CI during project stabilization, not during the current
  fast feature-development phase.
- If hosted CI is reintroduced, start with lightweight `checks` only, then add
  `default-regression`, and keep `remote-push-odb` out of push/PR gating until
  the heavy dependency cache and Gateway/Auth link surface are stable.
- Add artifact upload for CTest/build logs before re-enabling any heavy hosted
  job.
- Add deployment packaging only after service process boundaries and schema
  migration are stable.

## Verification

Local verification on 2026-06-05:

```bash
bash -n scripts/ci/checks.sh scripts/ci/default_regression.sh scripts/ci/remote_push_odb.sh
scripts/ci/checks.sh
scripts/ci/default_regression.sh
scripts/ci/remote_push_odb.sh
```

Results:

- Shell syntax check passed.
- Repository checks passed.
- Default no-ODB/no-gRPC regression passed:
  `RedisHiredisTest` and `AuthTokenTest`, 2/2 tests.
- Remote Push ODB/gRPC regression passed:
  - process-level `RemotePushGatewayServerSmokeTest`: 1/1 test;
  - Push-focused tests: 10/10 tests;
  - full ODB + Gateway + Push gRPC suite: 23/23 tests.

Note: the first sandboxed default-regression run failed because local CMake
uses `/home/myself/pkgs/vcpkg` and vcpkg could not take its filesystem lock
inside the restricted sandbox. The same script passed after allowing the
existing vcpkg cache/root to be used.

Remote Push ODB/gRPC validation note: an initial run exposed that building only
`test_remote_push_gateway_server_smoke` was not enough before `ctest -R Push`;
CTest had registered other Push tests whose executables had not been built.
`scripts/ci/remote_push_odb.sh` now builds the smoke target first, runs that
process-level smoke, then builds all targets before the Push-focused and full
CTest passes. The fixed script passed the full local remote Push ODB/gRPC
verification.

Hosted GitHub Actions follow-up on 2026-06-05:

- Run `26988119603` exposed stale protobuf generated output on the hosted
  runner and hidden whitespace-check diagnostics.
- Run `27009823882` confirmed protobuf regeneration worked but exposed the
  shallow-checkout whitespace-check issue.
- Run `27009928862` confirmed `checks` passed and `remote-push-odb` was skipped
  on push as designed, then exposed jwt-cpp JSON-traits API drift in
  `gateway/auth`.
- Run `27010726978` confirmed hosted compilation reached CTest, then exposed
  the missing Redis dependency in default-regression.
- Run `27011578634` confirmed hosted push CI was green:
  `checks` succeeded, `default-regression` succeeded, and `remote-push-odb`
  was skipped on push as designed.
- Run `27016533379` showed manual hosted `remote-push-odb` failed before
  project build because Ubuntu 24.04 apt does not provide `bdep`/`build2`.
- Run `27018217941` showed the build2 install approach needed the correct
  official download path.
- Run `27018375823` showed first-run ODB runtime paths were incorrectly
  canonicalized to `/installed` and `/src`; `scripts/build_odb_runtime_2_5.sh`
  was fixed to create and canonicalize target directories directly.
- Run `27018526777` ran for 1h15m and then failed in the Gateway/Auth link
  stage with unresolved `MultiPlatformAuthManager` and `PlatformTokenStrategy`
  symbols. This confirmed hosted heavy CI is not a good use of current
  iteration time.

Decision on 2026-06-06:

- Remove `.github/workflows/ci.yml`.
- Keep local CI scripts.
- Resume feature development; revisit hosted CI near final hardening.
