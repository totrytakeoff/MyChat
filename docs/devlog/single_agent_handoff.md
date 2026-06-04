# Single Agent Handoff

Date: 2026-06-04

Updated: 2026-06-05

## Status Summary

MyChat is ready for the next single coding agent.

Continue as a manual single agent unless the human explicitly asks for a
different workflow.

This document supersedes the earlier 2026-06-03 handoff. The old recommended
next task was "Finalize Friend Service MVP contract/docs, then start Group MVP";
that work has now been completed and verified.

Latest reliable baseline after this handoff update:

- Full ODB + Gateway + Push gRPC configure/build/test passed on 2026-06-05:
  20/20 tests in `build/remote-push-odb`.
- No-ODB default configure/build/test re-verified on 2026-06-05: 3/3 tests.
- Codec explicit-enable configure/generate/build passed:
  `generate_proto` and `im_codec_service`.
- Push gRPC explicit-enable configure/generate/build/test passed:
  `generate_proto`, `im_push_grpc_service`, `push_server`, `gateway_server`,
  `PushRuntimeTest`, `PushGrpcServiceTest`, `PushServerAppTest`,
  `PushServerRemoteAdaptersTest`, `GatewayPushDeliveryServiceTest`, and
  `RemotePushNotifierTest`.
- Redis/PostgreSQL Docker containers were restarted and are healthy.
- FanoutPolicy production implementations are complete.
- Friend Service and Friend HTTP controller compile, link, and have focused
  service/Gateway HTTP tests registered and passing.
- Group Service, Group HTTP controller, Group Message Service, and Group Message
  HTTP controller compile, link, and have focused service/Gateway HTTP tests
  registered and passing.
- This baseline was re-verified on 2026-06-04 after the Gateway structure
  cleanup, using `/tmp/mychat-build-review` for the ODB-enabled path.
- Gateway structure cleanup is complete in the working tree: HTTP controllers
  moved to `gateway/http`, push/fanout code moved to `gateway/push`, and
  WebSocket message handling moved to `gateway/ws`.
- Codec/gRPC generation chain cleanup is complete in the working tree:
  canonical generated protobuf/gRPC files are under `common/proto`,
  `generate_push_grpc` now exists beside the legacy-compatible
  `generate_codec_grpc`, and `im_codec_service` no longer compiles duplicated
  generated files from `services/codec`.
- Push Service boundary cleanup is complete in the working tree:
  `services/push` owns the `im::service::push::PushNotifier` interface,
  service-owned fanout policies, and `im::push_service` CMake target. Gateway
  WS and Group Message HTTP call sites now depend on that boundary.
- Push runtime core extraction is complete in the working tree:
  `services/push/PushRuntime` owns fanout selection, protobuf payload
  construction, send orchestration, and delivered marking decisions. The
  current in-process `gateway/push/PushService` is now a Gateway adapter around
  `ConnectionManager`, `WebSocketServer`, and Message Service delivered
  marking.
- Gateway Push/WS build gating was tightened after the boundary move:
  Message HTTP remains independently gated on Message Service; `PushService`,
  `MessageWsHandler`, and Group Message HTTP fanout now require
  `im::push_service`. A `MYCHAT_BUILD_SERVICES=OFF` Gateway-only
  configure/build smoke passed.
- Push gRPC remote boundary is present in the working tree:
  `common/proto/push.proto` defines `im.push.PushService.NotifyUser`,
  `common/proto/push.grpc.pb.*` is generated through CMake `generate_proto`,
  and `services/push/PushGrpcService` adapts gRPC requests to the existing
  `PushNotifier` boundary behind `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
- Gateway remote PushNotifier wiring is present in the working tree:
  `gateway/push/RemotePushNotifier` wraps the generated
  `im.push.PushService::Stub`; `GatewayServer` selects local vs. remote through
  `push.mode`; `config/dev.json` defaults to `push.mode = "local"` so current
  in-process delivery remains the default.
- Standalone Push server process slice is present in the working tree:
  `services/push/push_server` hosts `PushGrpcService + PushRuntime` and reads
  `push.listen_address` from config. It uses remote-aware Gateway delivery
  adapters when `push.gateway_delivery_endpoint` is configured and no-op
  adapters otherwise.
- Remote Push delivery callback channel first slice is present in the working
  tree: `GatewayPushDeliveryService` exposes Gateway-owned session lookup,
  payload send, and delivered marking on
  `push.gateway_delivery_listen_address`; `push_server` can call it through
  `push.gateway_delivery_endpoint` while Gateway keeps WebSocket session
  ownership.

## Important Repository Context

The latest Push boundary cleanup is currently in the working tree and may be
uncommitted. Preserve unrelated human or agent changes if new work starts from
a dirty tree.

Current important moved/new areas from the recent cleanup include:

- `gateway/README.md`
- `config/dev.json`
- `gateway/push/push_service.hpp`
- `gateway/push/push_service.cpp`
- `gateway/push/remote_push_notifier.hpp`
- `gateway/push/remote_push_notifier.cpp`
- `gateway/http/friend_http_controller.hpp`
- `gateway/http/friend_http_controller.cpp`
- `gateway/http/group_http_controller.hpp`
- `gateway/http/group_http_controller.cpp`
- `gateway/http/group_message_http_controller.hpp`
- `gateway/http/group_message_http_controller.cpp`
- `gateway/http/message_http_controller.hpp`
- `gateway/http/message_http_controller.cpp`
- `gateway/http/user_http_controller.hpp`
- `gateway/http/user_http_controller.cpp`
- `gateway/ws/message_ws_handler.hpp`
- `gateway/ws/message_ws_handler.cpp`
- `services/push/push_notifier.hpp`
- `services/push/fanout_policy.hpp`
- `services/push/fanout_policy.cpp`
- `services/push/push_runtime.hpp`
- `services/push/push_runtime.cpp`
- `services/push/push_grpc_service.hpp`
- `services/push/push_grpc_service.cpp`
- `services/push/push_server_adapters.hpp`
- `services/push/push_server_adapters.cpp`
- `services/push/push_server_app.hpp`
- `services/push/push_server_app.cpp`
- `services/push/push_server_main.cpp`
- `services/push/CMakeLists.txt`
- `services/friend/`
- `services/group/`
- `common/proto/codec_service.grpc.pb.cc`
- `common/proto/codec_service.grpc.pb.h`
- `common/proto/push.proto`
- `common/proto/push.pb.cc`
- `common/proto/push.pb.h`
- `common/proto/push.grpc.pb.cc`
- `common/proto/push.grpc.pb.h`
- `services/odb/friend.hpp`
- `services/odb/generated/friend-odb.*`
- `services/odb/generated/friend.sql`
- `services/odb/group.hpp`
- `services/odb/group_message.hpp`
- `services/odb/generated/group-odb.*`
- `services/odb/generated/group.sql`
- `services/odb/generated/group_message-odb.*`
- `services/odb/generated/group_message.sql`
- `docs/devlog/phase11_fanout_policies.md`
- `docs/devlog/phase12_friend_service_mvp.md`
- `docs/devlog/phase13_group_service_mvp.md`
- `docs/devlog/single_agent_handoff.md`
- `test/friend/`
- `test/gateway_friend/`
- `test/group/`
- `test/gateway_group/`
- `test/gateway_group_message/`
- `test/push/`

Latest cleanup details:

- Removed tracked clangd cache files from
  `gateway/message_processor/test/.cache/clangd/index`.
- Added `.gitignore` patterns for local index/build helper artifacts:
  `**/.cache/`, `.clangd/`, and `compile_commands.json`.
- Added `gateway/README.md` as the current Gateway module map.
- Kept historical demo/test directories in place:
  `gateway/auth/test`, `gateway/connection_manager/test`,
  `gateway/message_processor/demo`, `gateway/message_processor/test`, and
  `gateway/router/test_router_manager.cpp`.
- Did not find a `gateway/fanout_policies.cpp.cpp...` souvenir file during
  cleanup; preserve it if it appears later.
- Cleanup verification:
  - `git ls-files gateway | rg '/\.cache/|clangd'` returned no tracked cache
    files.
  - old Gateway include/path search for moved root-level controller/push/ws
    headers returned no active `gateway/` or `test/` hits.

Latest codec/gRPC cleanup details:

- Added vcpkg feature `codec-grpc` for optional gRPC runtime/plugin deps.
- Root CMake now has explicit `generate_common_proto`,
  `generate_codec_service_grpc`, `generate_push_grpc`, legacy-compatible
  `generate_codec_grpc`, and aggregate `generate_proto` targets.
- `common/proto/codec_service.grpc.pb.*` is generated from
  `common/proto/codec_service.proto`.
- `common/proto/push.grpc.pb.*` is generated from `common/proto/push.proto`.
- `im::proto` compiles `common/proto/codec_service.pb.cc`.
- `im_codec_service` compiles `common/proto/codec_service.grpc.pb.cc` and
  links `im::proto`; it no longer compiles `services/codec/base.pb.cc`.
- The inactive duplicate generated files under `services/codec` still exist,
  but are out of the active build path.

Latest Push boundary cleanup details:

- Added `MYCHAT_BUILD_PUSH_SERVICE` root CMake option, ON by default.
- Added `MYCHAT_BUILD_PUSH_GRPC_SERVICE` root CMake option, OFF by default.
- Added `services/push` with `im::service::push::PushNotifier`, service-owned
  fanout policies, and the `im::push_service` target.
- Added `im::push_grpc_service` gated by `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
- Moved `AllSessionsFanoutPolicy`, `PlatformFilterFanoutPolicy`, and
  `NewestSessionFanoutPolicy` into `services/push/fanout_policy.*`.
- `gateway/push/PushService` implements `PushNotifier`, maps Gateway
  `DeviceSessionInfo` values into service `PushSessionInfo` values through the
  `PushSessionProvider` adapter, sends encoded payloads through WebSocketServer,
  and marks delivery through Message Service.
- `services/push/PushRuntime` is the service-side runtime core. It owns fanout,
  `CMD_PUSH_MESSAGE` payload construction, best-effort send orchestration, and
  the “mark delivered after at least one successful send” rule.
- `MessageWsHandler` and `GroupMessageHttpController` now depend on
  `PushNotifier*` instead of concrete `PushService*`.
- Gateway CMake now uses separate compile definitions:
  `IM_ENABLE_MESSAGE_HTTP` for Message HTTP, `IM_ENABLE_PUSH_SERVICE` for the
  Gateway runtime adapter, and `IM_ENABLE_MESSAGE_WS` for WebSocket send
  handling. This avoids compiling Push/WS code when `services/push` is not
  built.
- Added characterization tests:
  - `GatewayMessageWsTest.SuccessfulSendNotifiesReceiverThroughBoundary`
  - `GatewayGroupMessageHttpTest.SendMessageFansOutToOtherMembersOnly`
- Added service-side runtime tests:
  - `PushRuntimeTest`
- Added Push gRPC adapter tests:
  - `PushGrpcServiceTest`
- Added Gateway remote PushNotifier client:
  - `gateway/push/RemotePushNotifier` implements
    `im::service::push::PushNotifier` through the generated
    `im.push.PushService::Stub`.
  - Gateway links this client only when `im::push_grpc_service` exists and
    defines `IM_ENABLE_REMOTE_PUSH_NOTIFIER`.
  - `GatewayServer` keeps one `PushNotifier*` injection point for
    `MessageWsHandler` and `GroupMessageHttpController`. It selects the
    in-process `PushService` by default and the remote client only when
    `push.mode = "remote"` is configured.
  - `RemotePushNotifierTest` covers request mapping and best-effort
    non-throwing behavior for RPC failures, non-success `BaseResponse`, and
    missing test client injection.
- Added standalone Push server process slice:
  - `push_server` executable target is built only when
    `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
  - `services/push/PushServerApp` hosts `PushGrpcService + PushRuntime` on
    `push.listen_address` (`0.0.0.0:9101` in `config/dev.json`).
  - `services/push/push_server_main.cpp` provides config, log-level, service
    identity, and signal-handler wiring.
  - `services/push/push_server_adapters.*` keeps explicit no-session/no-op
    adapters when no Gateway delivery endpoint is configured, and uses
    remote-aware Gateway delivery adapters when
    `push.gateway_delivery_endpoint` is configured.
  - `PushServerAppTest` covers binding an ephemeral port and calling
    `NotifyUser` through the generated gRPC stub.
  - `GatewayPushDeliveryServiceTest` and `PushServerRemoteAdaptersTest` cover
    the first Gateway callback channel for session lookup, payload send, and
    delivered marking.

## Recent Cleanup And Audit

The previous model left Friend Service/Gateway Friend HTTP integration in a
build-broken state. The following fixes were made before this handoff:

- `services/friend/friend_repository.hpp`
  - added required `<cstdint>` include;
  - changed `FriendRepository::create(const Friend&)` to `create(Friend)`.
- `services/friend/friend_repository.cpp`
  - persisted a mutable `Friend` object so ODB `auto id` can assign the id;
  - compared ODB enum columns directly to `FriendStatus` values instead of
    `static_cast<int>(...)`.
- `services/friend/friend_service.hpp`
  - added required `<cstdint>` include.
- `gateway/gateway_server/gateway_server.cpp`
  - included Friend HTTP/service headers under `IM_ENABLE_FRIEND_HTTP`;
  - made ODB database initialization include the Friend HTTP build flag;
  - initialized `FriendService` and `FriendHttpController`;
  - registered Friend HTTP routes before the legacy catch-all routes.
- `gateway/CMakeLists.txt`
  - linked `im_gateway_core` against `im::friend_service` when Friend HTTP is
    enabled, fixing final executable undefined references.

Friend HTTP routes now registered when the target is enabled:

- `POST /api/v1/friends/request`
- `POST /api/v1/friends/respond`
- `GET /api/v1/friends`
- `GET /api/v1/friends/pending`

This handoff audit then re-read current project state, verified that Friend and
Group work had advanced beyond the older handoff, and reconciled durable docs
to the current 20/20 full ODB + Gateway + Push gRPC and 3/3 no-ODB baseline.

## Current Product State

MyChat is a C++20 instant messaging system being rebuilt as staged
microservices. Preserve service boundaries, but keep each task narrow and
independently testable.

Completed and verified areas:

- Staged CMake build options and vcpkg manifest.
- Docker Redis/PostgreSQL development dependencies.
- Hiredis-backed Redis wrapper.
- JWT access/refresh token auth with Redis-backed per-token/per-JTI storage.
- PostgreSQL/ODB 2.5.0 user persistence.
- User Service MVP: register, login, profile.
- Gateway/User HTTP integration:
  - `POST /api/v1/auth/register`
  - `POST /api/v1/auth/login`
  - `GET /api/v1/auth/info`
- Message Service persistence core:
  - one-to-one text send;
  - chronological conversation history;
  - offline pull;
  - delivered/read marking.
- Gateway Message HTTP integration:
  - `POST /api/v1/messages/send`
  - `GET /api/v1/messages/history`
  - `GET /api/v1/messages/offline`
- Gateway WebSocket send/ack for `CMD_SEND_MESSAGE`.
- Online WebSocket push delivery via `CMD_PUSH_MESSAGE`.
- `PushService` extracted from `MessageWsHandler`.
- `PushService` has a pluggable `FanoutPolicy`.
- Production fanout policies are complete:
  - `PlatformFilterFanoutPolicy`
  - `NewestSessionFanoutPolicy`
  - default `AllSessionsFanoutPolicy` remains unchanged.
- Friend Service MVP is complete:
  - ODB-backed friend request model, repository, service, Gateway HTTP
    controller, service tests, and Gateway HTTP tests.
  - Routes: `POST /api/v1/friends/request`, `POST /api/v1/friends/respond`,
    `GET /api/v1/friends`, `GET /api/v1/friends/pending`.
- Group Service MVP is complete:
  - ODB-backed group/group_member model, GroupRepository, GroupService,
    Gateway Group HTTP controller, GroupMessage model/service, Gateway Group
    Message HTTP controller, and focused tests.
  - Service-level tests: 17 total cases in `test/group/test_group_service.cpp`
    (14 `GroupServiceTest` cases + 3 `GroupMessageServiceTest` cases).
  - Gateway Group HTTP tests: 23 cases.
  - Gateway Group Message HTTP tests: 15 cases.
  - Routes: `POST /api/v1/groups`, `POST /api/v1/groups/join`,
    `POST /api/v1/groups/leave`, `GET /api/v1/groups`,
    `GET /api/v1/groups/members`, `POST /api/v1/groups/messages/send`,
    `GET /api/v1/groups/messages/history`.
  - Group messages fan out to all other group members through
    `PushService::push_to_user`.
  - `GroupService::list_members` still returns an empty vector for
    nonexistent groups or non-member callers, but the Gateway HTTP controller
    maps those cases to `404` and `403` respectively.

Recently reconciled:

- `current_progress.md`, `roadmap.md`, `todo.md`, and `project_context.md`
  now match the verified Group-complete state.
- `single_agent_handoff.md` now records the 20/20 full ODB + Push gRPC and 3/3
  no-ODB baselines.
- `phase14_codec_grpc_generation_chain.md` records the completed codec/gRPC
  generation-chain cleanup.
- Follow-up review fixes were applied: group creation is atomic with owner
  insertion, GroupMessageService validates group/sender membership before
  persisting, and Friend/Group/GroupMessage tests clean generated UID/group
  rows without leaving orphan test data.
- The selected next step is not another boundary-definition task, not another
  standalone-binary task, and not another first-slice callback task. The
  `PushNotifier` boundary, service-owned fanout policies, `PushRuntime`, gRPC
  adapter, Gateway remote client, `push_server` target, and Gateway callback
  delivery channel are in place. Continue by adding an end-to-end remote Push
  smoke and hardening endpoint startup/config behavior.

## Verification Commands And Results

Dependencies started:

```bash
docker start mychat-redis mychat-postgres
```

Equivalent project command:

```bash
docker compose up -d redis postgres
```

Latest full ODB + Gateway + Push gRPC baseline, verified passing:

```bash
cmake -S . -B build/remote-push-odb \
  -DVCPKG_MANIFEST_FEATURES="pgsql-odb;codec-grpc" \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
TMPDIR=/home/myself/workspace/MyChat/build/tmp \
  cmake --build build/remote-push-odb -j2
ctest --test-dir build/remote-push-odb --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 20
```

Latest no-ODB/no-gRPC default baseline, verified passing:

```bash
cmake -S . -B build/default-regression \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/default-regression -j2
ctest --test-dir build/default-regression --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 3
```

Standalone Push server focused re-verification:

```bash
git diff --check
TMPDIR=/home/myself/workspace/MyChat/build/tmp \
  cmake --build build/remote-push-odb --target push_server test_push_server_app -j2
ctest --test-dir build/remote-push-odb -R Push --output-on-failure
```

Result:

```text
git diff --check passed.
push_server, test_push_server_app, test_gateway_push_delivery_service, and
test_push_server_remote_adapters built.
PushRuntimeTest, PushGrpcServiceTest, PushServerAppTest,
PushServerRemoteAdaptersTest, GatewayPushDeliveryServiceTest,
RemotePushNotifierTest, and PushServiceTest passed, 7/7.
```

Observed warnings during builds were existing unused-parameter warnings in
network/Gateway legacy code; no Push/gRPC build errors were found.

Operational notes:

- Running `ctest` inside the restricted sandbox can fail with socket errors.
  Use the approved/escalated run path when tests need Redis/PostgreSQL sockets.
- Running CMake can trigger vcpkg lock writes under `/home/myself/pkgs/vcpkg`;
  in restricted sandbox this may require approval.
- If tests fail with `Connection refused`, check that `mychat-redis` and
  `mychat-postgres` are running and ports 6379/5432 are listening.

## Important Context Files

Read these before coding:

- `docs/devlog/current_progress.md`
- `docs/devlog/phase11_fanout_policies.md`
- `docs/devlog/phase12_friend_service_mvp.md`
- `docs/devlog/phase13_group_service_mvp.md`
- `docs/devlog/single_agent_handoff.md`
- `docs/agent_context/project_context.md`
- `docs/agent_context/architecture_analysis.md`
- `docs/agent_context/roadmap.md`
- `docs/agent_context/todo.md`
- `docs/architecture/mvp_architecture.md`
- `docs/architecture/service_mvp_roadmap.md`

Useful latest task records:

- `docs/agent_context/tasks/task008/final.md`
- `docs/agent_context/tasks/task008/summary.md`
- `docs/agent_context/tasks/task008/review.md`

## Recommended Next Work

Selected next task:

```text
Add a remote Push end-to-end smoke across Gateway and standalone push_server,
then harden remote endpoint startup/config behavior.
```

Why this is the right next task:

- Friend and Group MVPs are now present, documented, and passing.
- Group message fanout is behind `PushNotifier`, and fanout policy plus runtime
  orchestration are already service-owned under `services/push`.
- Gateway layout is now clean enough to separate `gateway/push` from HTTP and
  WebSocket handling without first doing another file-structure task.
- Push gRPC contract generation is deterministic from canonical
  `common/proto` inputs: `generate_push_grpc` and aggregate `generate_proto`
  now produce `common/proto/push.grpc.pb.*`.
- Direct-message and group-message push/fanout behaviors plus service-side
  `PushRuntime` behavior are now pinned by focused tests, so the next boundary
  decision has a safer regression surface.
- The gRPC server adapter, `push_server` binary, Gateway
  `RemotePushNotifier`, and `GatewayPushDeliveryService` callback channel are
  present. What is missing is an end-to-end smoke that proves the two-process
  path preserves direct/group fanout behavior, plus startup/config hardening
  for common remote-mode mistakes.

Suggested scope:

- Audit the remote-mode boot path and identify the smallest existing test
  harness that can start `push_server` plus Gateway remote push wiring.
- Add an end-to-end local smoke that starts `push_server`, configures Gateway
  `push.mode=remote`, and verifies direct/group fanout still flows through the
  `PushNotifier` boundary.
- Add config/startup tests for missing `push.remote_endpoint`, missing
  `push.gateway_delivery_listen_address`, bind failure, and unavailable
  `push_server` where feasible without broad process orchestration.
- Keep current direct-message, group-message, `PushRuntimeTest`, and
  `PushGrpcServiceTest`/`PushServerAppTest`/`PushServerRemoteAdaptersTest`/
  `GatewayPushDeliveryServiceTest`/`RemotePushNotifierTest` characterization
  tests passing through each move.
- Preserve Gateway HTTP/WS wire contracts and sender ack behavior.
- Keep `MYCHAT_BUILD_CODEC_SERVICE=OFF` and `MYCHAT_BUILD_PUSH_GRPC_SERVICE=OFF`
  default behavior unchanged; explicit gRPC builds should require the
  `codec-grpc` vcpkg feature.

Out of scope for the next task:

- Do not redo Friend or Group MVP work.
- Do not change Friend/Group public API contracts unless a failing test proves
  they are wrong.
- Do not start a broad Push microservice rewrite; build on `PushRuntime` and
  keep adapter responsibilities explicit.
- Do not rewrite the already-added `push_server` process target from scratch.
  It is a valid transitional slice; evolve the adapters and remote channel.
- Do not change `MessageWsHandler` ack behavior.
- Do not change default `AllSessionsFanoutPolicy` behavior.
- Do not remove inactive duplicate `services/codec/*.pb.*` files unless a
  focused legacy include-path check is part of the task.
- Regenerate codec/gRPC artifacts only through the documented CMake target and
  only with the CMake-selected vcpkg `protoc`.
- Do not refactor `pgsql_conn.hpp` unless it blocks the chosen next task.
- Do not enable legacy test suites wholesale.
- Preserve any nested `gateway/fanout_policies.cpp.cpp...` souvenir file if it
  appears in the workspace. It was not present during the final review, but the
  human explicitly wants it preserved.

Recommended sequence:

1. Audit current `push_server`/`RemotePushNotifier`/`GatewayPushDeliveryService`
   flow and pick a minimal end-to-end smoke shape.
2. Add a remote Push E2E smoke with `push.mode=remote` and standalone
   `push_server`.
3. Harden endpoint config/startup behavior and document the expected local
   topology.
4. Keep direct-message, group-message, PushRuntime, PushGrpcService,
   PushServerApp, PushServerRemoteAdapters, GatewayPushDeliveryService, and
   RemotePushNotifier tests green.
6. Redis connection pool before performance/load work.
7. Schema migration framework before broad persistence evolution.

## Known Risks

- Inactive duplicate generated files still exist under `services/codec`, but
  they are out of the active build path.
- ODB 2.5.0 runtime setup is manual. CMake expects runtime under
  `.odb/installed` or `MYCHAT_ODB_ROOT`.
- No schema migration framework exists yet.
- Redis wrapper is single-connection and mutex-serialized.
- Legacy tests are gated and may fail if re-enabled wholesale.
- `pgsql_conn.hpp` has pre-existing string-ID/template/raw-pointer issues.
  Current services bypass it with direct `odb::pgsql::database` usage.
- Friend and Group MVP docs were reconciled in this handoff. If future edits
  touch these areas, preserve the verified 20/20 full ODB + Push gRPC and 3/3
  no-ODB baselines.

## Prompt For Next Agent

```text
You are taking over the MyChat project as a single coding agent.

Repository: /home/myself/workspace/MyChat

Project summary:
MyChat is a C++20 instant messaging system being rebuilt as staged
microservices. Preserve service boundaries. Keep each task narrow,
independently testable, and aligned with existing CMake/ODB patterns.

Before coding, read:
- docs/devlog/single_agent_handoff.md
- docs/devlog/current_progress.md
- docs/devlog/phase11_fanout_policies.md
- docs/devlog/phase12_friend_service_mvp.md
- docs/devlog/phase13_group_service_mvp.md
- docs/agent_context/project_context.md
- docs/agent_context/architecture_analysis.md
- docs/agent_context/roadmap.md
- docs/agent_context/todo.md
- docs/architecture/mvp_architecture.md
- docs/architecture/service_mvp_roadmap.md

Current reliable state:
- FanoutPolicy production work is complete and service-owned under
  `services/push`.
- Full ODB + Gateway + Push gRPC baseline passed 20/20 on 2026-06-05 in
  `build/remote-push-odb`.
- No-ODB default baseline passed 3/3 on 2026-06-05 after Redis was started.
- Gateway layout cleanup is complete: HTTP controllers are under
  gateway/http, push/fanout code is under gateway/push, and WebSocket message
  handling is under gateway/ws.
- PushNotifier boundary cleanup is complete: `services/push` owns the boundary,
  fanout policies, and `PushRuntime`; Gateway call sites depend on
  `PushNotifier`, and current PushService is a narrow Gateway adapter around
  session lookup, payload send, and delivered marking.
- Codec/gRPC generation chain cleanup is complete: common/proto is canonical,
  generate_proto covers active protobuf/gRPC generation in explicit gRPC
  builds, and im_codec_service no longer compiles duplicated generated files
  from services/codec.
- Push gRPC remote boundary is complete: common/proto/push.proto defines
  im.push.PushService.NotifyUser, generate_push_grpc produces
  common/proto/push.grpc.pb.*, and im::push_grpc_service adapts generated gRPC
  calls to PushNotifier behind MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON.
- Gateway remote PushNotifier wiring is complete: gateway/push/RemotePushNotifier
  wraps im.push.PushService::Stub; GatewayServer selects local vs. remote
  notifier through config push.mode; config/dev.json defaults to local.
- Standalone Push server process slice is complete: services/push/push_server
  hosts PushGrpcService + PushRuntime, reads push.listen_address, and has a
  PushServerAppTest. When push.gateway_delivery_endpoint is configured it uses
  remote-aware Gateway adapters; otherwise it keeps explicit no-session/no-op
  adapters for local transitional startup.
- Remote Push delivery callback channel first slice is complete:
  GatewayPushDeliveryService exposes Gateway-owned session lookup, payload
  send, and delivered marking on push.gateway_delivery_listen_address;
  push_server calls it through push.gateway_delivery_endpoint.
- Friend Service/Gateway Friend HTTP compile, link, and pass focused tests.
- Group Service/Gateway Group HTTP and Group Message HTTP compile, link, and
  pass focused tests.
- The latest Push/gRPC cleanup may be uncommitted in the working tree. Do not
  delete any nested fanout_policies.cpp.cpp... souvenir file if one appears.

Recommended next task:
Add a remote Push end-to-end smoke across Gateway and standalone push_server,
then harden remote endpoint startup/config behavior.

Constraints:
- Do not redo Friend or Group MVP work.
- Do not rewrite the already-added standalone push_server process target from
  scratch; evolve its adapters and remote channel.
- Do not introduce a broad Push microservice rewrite without a written boundary
  and focused test plan.
- Keep Gateway WS and Group Message HTTP call sites depending on
  `im::service::push::PushNotifier`, not concrete runtime implementation
  details.
- Do not change MessageWsHandler ack behavior.
- Do not change default AllSessionsFanoutPolicy behavior.
- Do not change current direct-message push semantics: successful online push
  marks delivered; offline recipients remain pullable.
- Do not change current group fanout semantics: fan out to other group members,
  not the sender.
- Use common/proto as the canonical proto source tree for any generated
  contract updates.
- Do not use shell protoc for checked-in generated files; use the
  CMake-selected vcpkg protoc path through generate_proto.
- Keep MYCHAT_BUILD_CODEC_SERVICE=OFF and MYCHAT_BUILD_PUSH_GRPC_SERVICE=OFF
  default behavior unchanged; explicit gRPC paths should use
  -DVCPKG_MANIFEST_FEATURES=codec-grpc.
- When building full ODB + gRPC locally, prefer an ignored repo build dir such
  as build/remote-push-odb and set TMPDIR to build/tmp if /tmp tmpfs quota is
  tight.
- Do not use DROP TABLE in tests.
- Preserve existing tests and CMake gating.
- Run both ODB-enabled and no-ODB build/test baselines when done.

Useful commands:
docker compose up -d redis postgres

cmake -S . -B /tmp/mychat-build-codec \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_CODEC_SERVICE=ON \
  -DMYCHAT_BUILD_GATEWAY=OFF \
  -DMYCHAT_BUILD_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-codec --target generate_proto -j2
cmake --build /tmp/mychat-build-codec --target im_codec_service -j2

cmake -S . -B /tmp/mychat-build-pushgrpc \
  -DVCPKG_MANIFEST_FEATURES=codec-grpc \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_CODEC_SERVICE=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-pushgrpc --target generate_proto -j2
cmake --build /tmp/mychat-build-pushgrpc --target im_push_grpc_service -j2
cmake --build /tmp/mychat-build-pushgrpc --target gateway_server -j2
cmake --build /tmp/mychat-build-pushgrpc --target push_server -j2
cmake --build /tmp/mychat-build-pushgrpc --target test_push_runtime -j2
cmake --build /tmp/mychat-build-pushgrpc --target test_push_grpc_service -j2
cmake --build /tmp/mychat-build-pushgrpc --target test_push_server_app -j2
cmake --build /tmp/mychat-build-pushgrpc --target test_remote_push_notifier -j2
ctest --test-dir /tmp/mychat-build-pushgrpc -R "Push" --output-on-failure

cmake -S . -B build/remote-push-odb \
  -DVCPKG_MANIFEST_FEATURES="pgsql-odb;codec-grpc" \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
mkdir -p build/tmp
TMPDIR=/home/myself/workspace/MyChat/build/tmp cmake --build build/remote-push-odb -j2
ctest --test-dir build/remote-push-odb --output-on-failure

cmake -S . -B /tmp/mychat-build-review-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review-noodb -j2
ctest --test-dir /tmp/mychat-build-review-noodb --output-on-failure

When done:
- Summarize changed files and behavior.
- Record exact verification commands and pass/fail results.
- Update docs/devlog/current_progress.md.
- Update docs/devlog/phase15_push_service_boundary.md or add the next Push
  remote-wiring devlog with changed files, tests, and remaining risks.
```
