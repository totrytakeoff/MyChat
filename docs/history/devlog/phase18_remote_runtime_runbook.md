# Phase 18: Remote Services Runtime Runbook

Date: 2026-06-07

## Purpose

Provide one concrete local runtime path for the distributed MyChat MVP. The
default development config remains local/in-process for speed, while this
runbook starts User, Message, Friend, Group, Push, and Gateway as separate
local processes with gRPC service boundaries enabled.

## Prerequisites

- ODB runtime is available at `.odb/installed` or `MYCHAT_ODB_ROOT`.
- The heavy ODB/gRPC build has been produced:

```bash
scripts/ci/remote_push_odb.sh
```

This builds the service server binaries, `gateway_server`, generated
protobuf/gRPC artifacts, and the full remote-services test suite.

## Config Files

- `config/dev.json`
  - Default local-mode development config.
  - `user.mode`, `message.mode`, `friend.mode`, `group.mode`, and `push.mode`
    are all `local`.
- `config/dev.remote-push.json`
  - Two-process Push remote config.
  - User/Message/Friend/Group remain local, Push is remote.
- `config/dev.remote-all.json`
  - Full local multi-process remote config.
  - User, Message, Friend, Group, and Push are all remote.
  - Gateway calls `*.remote_endpoint`; service servers listen on
    `*.listen_address`.
  - Push also uses `push.gateway_delivery_endpoint` to call back into Gateway's
    `GatewayPushDeliveryService`.

## Start Dependencies

Gateway and service binaries do not run migrations implicitly. Prepare Docker
dependencies and schema first:

```bash
scripts/dev/prepare_runtime.sh
```

That starts Redis/PostgreSQL when Docker Compose is available and applies
ordered PostgreSQL migrations through `scripts/db/migrate_postgres.sh`.

## Start Full Remote Stack

Preferred wrapper:

```bash
scripts/dev/run_remote_services_stack.sh
```

The wrapper uses:

- build dir: `build/remote-push-odb`
- config: `config/dev.remote-all.json`
- log dir: `build/dev-runtime`

Override examples:

```bash
MYCHAT_DEV_REMOTE_SERVICES_BUILD_DIR=build/remote-push-odb \
MYCHAT_DEV_REMOTE_SERVICES_CONFIG=config/dev.remote-all.json \
MYCHAT_DEV_LOG_DIR=build/dev-runtime \
scripts/dev/run_remote_services_stack.sh
```

The wrapper starts, in order:

1. `user_server`
2. `message_server`
3. `friend_server`
4. `group_server`
5. `push_server`
6. `gateway_server`

It stops all started processes when one exits or when Ctrl+C is pressed.

## Manual Startup

Manual startup is useful when inspecting logs in separate terminals:

```bash
scripts/dev/prepare_runtime.sh

build/remote-push-odb/services/user/user_server --config config/dev.remote-all.json
build/remote-push-odb/services/message/message_server --config config/dev.remote-all.json
build/remote-push-odb/services/friend/friend_server --config config/dev.remote-all.json
build/remote-push-odb/services/group/group_server --config config/dev.remote-all.json
build/remote-push-odb/services/push/push_server --config config/dev.remote-all.json
build/remote-push-odb/gateway/gateway_server --config config/dev.remote-all.json
```

## Endpoint Map

- User gRPC: `127.0.0.1:9001`
- Message gRPC: `127.0.0.1:9002`
- Friend gRPC: `127.0.0.1:9003`
- Group gRPC: `127.0.0.1:9004`
- Push gRPC: `127.0.0.1:9101`
- Gateway Push callback gRPC: `127.0.0.1:9102`
- Gateway HTTP: `http://127.0.0.1:8102`
- Gateway WebSocket: `wss://127.0.0.1:8101`

Health smoke:

```bash
curl -sS http://127.0.0.1:8102/api/v1/health
```

Expected response:

```json
{"status":"ok"}
```

## Verification

Canonical local regression before trusting the stack:

```bash
scripts/ci/checks.sh
scripts/ci/default_regression.sh
scripts/ci/remote_push_odb.sh
```

Focused full remote build check:

```bash
ctest --test-dir build/remote-push-odb --output-on-failure --parallel 1
```

Run ODB-backed CTest suites serially when they share the same Docker
PostgreSQL database. Build jobs may be parallel, but test execution for the
heavy remote-services suite should stay `--parallel 1` until test data is
isolated more strongly.

Latest runtime smoke:

```bash
scripts/dev/run_remote_services_stack.sh
curl -sS http://127.0.0.1:8102/api/v1/health
```

Result on 2026-06-07:

- Redis/PostgreSQL dependency preparation and schema migration completed.
- `user_server`, `message_server`, `friend_server`, `group_server`,
  `push_server`, `gateway_server`, and Gateway Push callback all listened on
  their configured local ports.
- Gateway health returned `{"status": "ok"}`.

## Troubleshooting

- Missing binaries: run `scripts/ci/remote_push_odb.sh` or override binary
  paths with `MYCHAT_DEV_*_SERVER_BIN`.
- Docker permission errors: start Redis/PostgreSQL manually or run the script
  in an environment that can access Docker.
- Port conflicts: stop older local Gateway/service processes, then rerun the
  wrapper. Logs are under `build/dev-runtime`.
- If a terminal or tool session is closed while a wrapper is running, the
  wrapper traps `HUP`, `INT`, and `TERM` and attempts to stop child processes.
  If ports remain occupied, inspect `ss -ltnp` for ports `9001-9004`,
  `9101-9102`, and `8101-8102`.
- Push starts but delivery does not complete: check that
  `push.gateway_delivery_endpoint` in the Push process matches
  `push.gateway_delivery_listen_address` in Gateway config.
