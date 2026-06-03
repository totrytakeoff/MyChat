# Single Agent Handoff

Date: 2026-06-03

## Status Summary

MyChat is ready for the next single coding agent.

The Codgent multi-agent workflow remains paused intentionally. Continue as a
manual single agent unless the human explicitly asks to resume Codgent.

This document supersedes the earlier 2026-06-03 handoff. The old recommended
next task was "Finalize Friend Service MVP contract/docs, then start Group MVP";
that work has now been completed and verified.

Latest reliable baseline after this handoff update:

- ODB-enabled configure/build/test passed: 14/14 tests.
- No-ODB configure/build/test passed: 2/2 tests.
- Redis/PostgreSQL Docker containers were restarted and are healthy.
- FanoutPolicy production implementations are complete.
- Friend Service and Friend HTTP controller compile, link, and have focused
  service/Gateway HTTP tests registered and passing.
- Group Service, Group HTTP controller, Group Message Service, and Group Message
  HTTP controller compile, link, and have focused service/Gateway HTTP tests
  registered and passing.
- This baseline was re-verified on 2026-06-03 after the final review/cleanup
  pass, using `/tmp/mychat-build-review` and a freshly recreated
  `/tmp/mychat-build-review-noodb`.

## Important Working Tree Context

The working tree contains many uncommitted changes and untracked files from the
previous agent and the current cleanup. Do not revert them without explicit
human approval.

Current important untracked/new areas include:

- `gateway/fanout_policies.hpp`
- `gateway/fanout_policies.cpp`
- `gateway/friend_http_controller.hpp`
- `gateway/friend_http_controller.cpp`
- `gateway/group_http_controller.hpp`
- `gateway/group_http_controller.cpp`
- `gateway/group_message_http_controller.hpp`
- `gateway/group_message_http_controller.cpp`
- `services/friend/`
- `services/group/`
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

Tracked files also have modifications in CMake, gateway, agent context docs,
and PushService tests.

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
to the current 14/14 ODB and 2/2 no-ODB baseline.

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
- `single_agent_handoff.md` now records the 14/14 ODB and 2/2 no-ODB baselines.
- Follow-up review fixes were applied: group creation is atomic with owner
  insertion, GroupMessageService validates group/sender membership before
  persisting, and Friend/Group/GroupMessage tests clean generated UID/group
  rows without leaving orphan test data.
- The selected next step is not Friend/Group. It is Task 009:
  clean the codec/gRPC generation chain from canonical `common/proto` inputs
  before standalone Push Service work.

## Verification Commands And Results

Dependencies started:

```bash
docker start mychat-redis mychat-postgres
```

Equivalent project command:

```bash
docker compose up -d redis postgres
```

ODB-enabled baseline, verified passing:

```bash
cmake -S . -B /tmp/mychat-build-review \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review -j2
ctest --test-dir /tmp/mychat-build-review --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 14
```

No-ODB baseline, verified passing:

```bash
cmake -S . -B /tmp/mychat-build-review-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review-noodb -j2
ctest --test-dir /tmp/mychat-build-review-noodb --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 2
```

Final review re-verification on 2026-06-03:

```bash
docker start mychat-redis mychat-postgres
cmake --build /tmp/mychat-build-review -j2
ctest --test-dir /tmp/mychat-build-review --output-on-failure
cmake -S . -B /tmp/mychat-build-review-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review-noodb -j2
ctest --test-dir /tmp/mychat-build-review-noodb --output-on-failure
```

Result:

```text
ODB-enabled: 100% tests passed, 0 tests failed out of 14.
No-ODB: 100% tests passed, 0 tests failed out of 2.
```

Observed warnings during the no-ODB build were existing unused-parameter
warnings in legacy/common Gateway code; no new build errors were found.

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
Clean the codec/gRPC generation chain before coding the standalone Push Service.
```

Why this is the right next task:

- Friend and Group MVPs are now present, documented, and passing.
- Group message fanout currently calls `PushService::push_to_user` in-process
  per member, which is correct for the staged MVP but not yet a standalone Push
  microservice boundary.
- `services/codec` generated gRPC/protobuf artifacts are split from the root
  `generate_proto` target. They currently build when gRPC is available, but
  they are not regenerated deterministically.
- CMake selects vcpkg `protoc-29.3.0`; shell `protoc` is 34.1. Mixing those
  would create incompatible checked-in generated files.
- The next implementation choice affects service boundaries, build gating, and
  test strategy more than it affects a single controller.

Suggested scope:

- Use `common/proto` as the only proto source tree.
- Add deterministic gRPC plugin discovery and generation targets.
- Make `generate_proto` cover both active protobuf and codec gRPC outputs.
- Remove duplicated `services/codec/base.pb.*` from the active build path.
- Keep `MYCHAT_BUILD_CODEC_SERVICE=OFF` default behavior unchanged.
- Verify `MYCHAT_BUILD_CODEC_SERVICE=ON` still builds `im_codec_service`.

Out of scope for the next task:

- Do not redo Friend or Group MVP work.
- Do not change Friend/Group public API contracts unless a failing test proves
  they are wrong.
- Regenerate codec/gRPC artifacts only through the documented CMake target and
  only with the CMake-selected vcpkg `protoc`.
- Do not introduce a broad Push microservice rewrite without a written boundary
  and focused test plan.
- Do not change `MessageWsHandler` ack behavior.
- Do not change default `AllSessionsFanoutPolicy` behavior.
- Do not refactor `pgsql_conn.hpp` unless it blocks the chosen next task.
- Do not enable legacy test suites wholesale.
- Preserve any nested `gateway/fanout_policies.cpp.cpp...` souvenir file if it
  appears in the workspace. It was not present during the final review, but the
  human explicitly wants it preserved.

Recommended sequence:

1. Codec/gRPC generation chain cleanup from canonical `common/proto` inputs.
2. Focused codec explicit-enable build verification.
3. Standalone Push Service boundary and migration plan.
4. Focused tests proving direct-message push and group-message fanout preserve
   current behavior.
5. Redis connection pool before performance/load work.
6. Schema migration framework before broad persistence evolution.

## Known Risks

- `services/codec` generated files are stale and gated OFF by default.
- ODB 2.5.0 runtime setup is manual. CMake expects runtime under
  `.odb/installed` or `MYCHAT_ODB_ROOT`.
- No schema migration framework exists yet.
- Redis wrapper is single-connection and mutex-serialized.
- Legacy tests are gated and may fail if re-enabled wholesale.
- `pgsql_conn.hpp` has pre-existing string-ID/template/raw-pointer issues.
  Current services bypass it with direct `odb::pgsql::database` usage.
- Friend and Group MVP docs were reconciled in this handoff. If future edits
  touch these areas, preserve the verified 14/14 ODB and 2/2 no-ODB baselines.

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
- FanoutPolicy production work is complete.
- ODB-enabled baseline passed 14/14.
- No-ODB baseline passed 2/2.
- Friend Service/Gateway Friend HTTP compile, link, and pass focused tests.
- Group Service/Gateway Group HTTP and Group Message HTTP compile, link, and
  pass focused tests.
- Current feature snapshot is committed at `e076fe6`. Do not delete any nested
  fanout_policies.cpp.cpp... souvenir file if one appears.

Recommended next task:
Clean the codec/gRPC generation chain before coding standalone Push.

Constraints:
- Do not redo Friend or Group MVP work.
- Use `common/proto` as the canonical proto source tree.
- Do not use shell `protoc` 34.1 for checked-in generated files; use the
  CMake-selected vcpkg `protoc-29.3.0`.
- Do not introduce a broad standalone Push microservice rewrite without a
  written boundary and focused test plan.
- Do not change MessageWsHandler ack behavior.
- Do not change default AllSessionsFanoutPolicy behavior.
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

cmake -S . -B /tmp/mychat-build-review \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review -j2
ctest --test-dir /tmp/mychat-build-review --output-on-failure

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
- Update docs/devlog/phase14_codec_grpc_generation_chain.md with the final
  generation-chain behavior.
```
