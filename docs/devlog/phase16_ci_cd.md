# Phase 16: CI/CD Engineering Baseline

Date: 2026-06-05

## Purpose

Promote the current local regression commands into reusable CI entrypoints
without changing project runtime behavior. The first CI/CD slice is split into
a lightweight default path and a heavier remote Push ODB/gRPC path.

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
  - Requires ODB 2.5.0 runtime at `.odb/installed` by default.
  - Can build the ODB runtime first when
    `MYCHAT_CI_BUILD_ODB_RUNTIME=ON`.
  - Builds `test_remote_push_gateway_server_smoke`, runs the process-level
    remote Push smoke, builds the full test set, then runs Push-focused tests
    and the full suite.

New GitHub Actions workflow:

- `.github/workflows/ci.yml`
  - `checks` runs repository JSON/diff hygiene.
  - `default-regression` runs on pushes to `main`/`master` and pull requests.
  - `remote-push-odb` is manual-only through `workflow_dispatch` with
    `run_remote_push_odb=true`, because it installs heavier dependencies,
    builds or restores ODB runtime, starts Docker Redis/PostgreSQL, and enables
    gRPC/ODB.

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

- Re-run the GitHub Actions workflow after each hosted runner compatibility
  fix until `checks` and `default-regression` are both green on push.
- Decide whether the heavy `remote-push-odb` job should eventually run on
  `main` pushes after dependency cache stability is proven.
- Add artifact upload for CTest logs if CI failures become hard to inspect.
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
