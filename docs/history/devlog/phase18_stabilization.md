# Phase 18: Final Stabilization And Regression Closure

Date: 2026-06-07

## Purpose

Move the project from feature-completion mode into final stabilization. The
major service MVPs and remote boundaries now exist; the remaining work is to
prove they are stable together, keep configuration behavior explicit, and make
the regression process repeatable.

## Current Stabilization Baseline

Completed and covered by focused tests:

- User, Message, Friend, Group, and Push service MVPs.
- Gateway HTTP and WebSocket entrypoints for the current MVP surface.
- PostgreSQL/ODB persistence for user, message, friend, group, and group
  message data.
- Redis token/session primitives and hiredis connection-pool behavior.
- gRPC service boundaries for User, Message, Friend, Group, and Push behind
  explicit build switches.
- Standalone server apps for User, Message, Friend, Group, and Push behind
  explicit gRPC builds.
- Gateway local/remote facades for User, Message, Friend, Group, and Push.
- Process-level remote Gateway smokes for User, Message, Friend, Group, and
  Push paths.

## Regression Entry Points

Lightweight default regression:

```bash
scripts/ci/checks.sh
scripts/ci/default_regression.sh
```

Heavy remote-services ODB/gRPC regression:

```bash
scripts/build_odb_runtime_2_5.sh
scripts/ci/remote_push_odb.sh
```

The heavy script name is historical. Its current purpose is no longer only
remote Push; it enables all active service gRPC boundaries:

- `MYCHAT_BUILD_USER_GRPC_SERVICE=ON`
- `MYCHAT_BUILD_MESSAGE_GRPC_SERVICE=ON`
- `MYCHAT_BUILD_FRIEND_GRPC_SERVICE=ON`
- `MYCHAT_BUILD_GROUP_GRPC_SERVICE=ON`
- `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`

It also regenerates all active protobuf/gRPC outputs through CMake
`generate_proto`, starts Redis/PostgreSQL when Docker Compose is available,
applies migrations, builds the full target set, and runs the full CTest suite.

## Local Runtime Topology

Default local mode:

- Build defaults keep gRPC service switches OFF unless explicitly enabled.
- `config/dev.json` keeps Gateway in local facade mode for fast development.
- Gateway and service binaries do not run PostgreSQL migrations implicitly.
- Run `scripts/dev/prepare_runtime.sh` before starting local processes.

Explicit remote mode:

- User, Message, Friend, Group, and Push each have standalone server apps behind
  their gRPC build switches.
- Gateway selects local or remote facades through per-service config such as
  `user.mode`, `message.mode`, `friend.mode`, `group.mode`, and `push.mode`.
- `config/dev.remote-push.json` currently documents the most complete local
  multi-process example for Push because Push needs the reverse Gateway
  delivery callback path.
- `config/dev.remote-all.json` and
  `scripts/dev/run_remote_services_stack.sh` provide the full local
  multi-process remote stack.
- `docs/devlog/phase18_remote_runtime_runbook.md` documents startup, endpoint
  ownership, health smoke, and troubleshooting.
- Full remote stack startup smoke passed on 2026-06-07: the wrapper started all
  five service servers plus Gateway, and Gateway health returned
  `{"status": "ok"}`.

## Important Test Isolation Rule

Do not run multiple ODB-backed CTest suites in parallel when they share the
same PostgreSQL database and overlapping test prefixes. The tests are stable
when run serially, but parallel runs can delete each other's prefixed rows and
produce false failures or PostgreSQL deadlock errors.

`scripts/ci/remote_push_odb.sh` now enforces serial CTest execution with
`--parallel 1`. Build parallelism is still controlled by `MYCHAT_CI_JOBS`; test
parallelism is deliberately not exposed for this ODB-backed heavy suite.

Safe pattern:

```bash
ctest --test-dir build/group-grpc-off-verify \
  -R "GatewayGroupHttpTest|GatewayGroupMessageHttpTest" \
  --output-on-failure --parallel 1

ctest --test-dir build/remote-push-odb \
  -R "RemoteGroupClientTest|RemoteGroupGatewayServerSmokeTest|GroupGrpcServiceTest|GatewayGroupHttpTest|GatewayGroupMessageHttpTest" \
  --output-on-failure --parallel 1
```

Avoid running those two commands concurrently against the same Docker
PostgreSQL instance.

## Current Verification Snapshot

Latest stabilization checks in this phase:

```bash
bash -n scripts/ci/remote_push_odb.sh scripts/ci/default_regression.sh \
  scripts/ci/checks.sh scripts/dev/prepare_runtime.sh \
  scripts/dev/run_remote_push_stack.sh
scripts/ci/checks.sh
```

Result: passed.

Latest serial-policy closure checks:

```bash
bash -n scripts/ci/remote_push_odb.sh scripts/ci/default_regression.sh \
  scripts/ci/checks.sh scripts/dev/run_remote_services_stack.sh \
  scripts/dev/run_remote_push_stack.sh scripts/dev/prepare_runtime.sh
git diff --check
scripts/ci/checks.sh
```

Result: passed.

```bash
ctest --test-dir build/remote-push-odb \
  -R "RemotePushGatewayEntrypointsTest|RemotePushGatewayServerSmokeTest|RemoteUserGatewayServerSmokeTest|RemoteMessageGatewayServerSmokeTest|RemoteFriendGatewayServerSmokeTest|RemoteGroupGatewayServerSmokeTest" \
  --output-on-failure --parallel 1
```

Result: remote Gateway focused smoke passed, 6/6 tests.

```bash
scripts/ci/default_regression.sh
```

Result: default no-ODB/no-gRPC regression passed, 2/2 tests:
`RedisHiredisTest` and `AuthTokenTest`.

```bash
MYCHAT_CI_REMOTE_PUSH_BUILD_DIR=build/remote-push-odb \
MYCHAT_CI_VCPKG_INSTALLED_DIR=/home/myself/workspace/MyChat/vcpkg_installed/remote-push-odb \
scripts/ci/remote_push_odb.sh
```

Result: the first heavy run exposed one stale test assembly:
`RemotePushGatewayEntrypointsTest` still constructed
`MessageWsHandler` and `GroupMessageHttpController` with direct service
objects after those production entrypoints had moved to Gateway
`MessageClient`/`GroupClient` facades. The test was updated to use
`LocalMessageClient` and `LocalGroupClient`, matching the production local
facade path. After that fix:

- `cmake --build build/remote-push-odb -j2` passed.
- `ctest --test-dir build/remote-push-odb -R Push --output-on-failure --parallel 1`
  passed, 10/10 tests.
- `ctest --test-dir build/remote-push-odb --output-on-failure --parallel 1`
  passed, 40/40 tests.

Earlier focused closure checks before this phase:

```bash
ctest --test-dir build/group-grpc-off-verify \
  -R "GatewayGroupHttpTest|GatewayGroupMessageHttpTest" \
  --output-on-failure
```

Result: 2/2 passed.

```bash
ctest --test-dir build/remote-push-odb \
  -R "RemoteGroupClientTest|RemoteGroupGatewayServerSmokeTest|GroupGrpcServiceTest|GatewayGroupHttpTest|GatewayGroupMessageHttpTest" \
  --output-on-failure
```

Result: 5/5 passed.

## Remaining Stabilization Work

1. Isolate ODB-backed test data more strongly before enabling parallel CTest
   for the heavy remote-services suite.
2. Revisit hosted GitHub CI only after local heavy regression is stable enough
   that CI work does not block final project cleanup.

## Constraints

- Preserve current HTTP/WS external contracts.
- Keep default builds gRPC-off and ODB-off unless explicitly configured.
- Keep checked-in proto/gRPC outputs generated through CMake `generate_proto`.
- Do not migrate service repositories from direct `odb::pgsql::database` to
  `PgSqlConnection` without a separate repository-boundary plan.
- Do not re-enable hosted CI during this stabilization pass unless explicitly
  requested.
