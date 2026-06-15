# Phase 13: Group Service MVP

## Summary

Implemented Group Service MVP with ODB schema (group + group_member), GroupRepository, GroupService (create/join/leave/list), Gateway HTTP controller, GroupMessage ODB model for group message storage, GroupMessageService, Gateway group message HTTP controller (send/history with member fanout), and focused tests at each layer.

## Architecture

Group and GroupMember share one ODB header (`services/odb/group.hpp`) with two `#pragma db object` classes, reducing ODB compiler runs. A separate `services/odb/group_message.hpp` was added for group message storage.

The creator is automatically added as OWNER member on group creation. Owner cannot leave (must transfer ownership first, not yet implemented). Group member listing requires the caller to be a member.

For fanout, the group message HTTP controller checks caller membership, stores the message via GroupMessageService, then calls PushService::push_to_user for each other member. GroupMessageService also validates group existence and sender membership before persisting, so the service layer remains safe if it is later exposed behind a standalone service boundary. PushService null-check is safe when no push service is available (tests pass nullptr).

### Routes

| HTTP | Route | Handler |
|---|---|---|
| POST | `/api/v1/groups` | create group |
| POST | `/api/v1/groups/join` | join group (body: group_id) |
| POST | `/api/v1/groups/leave` | leave group (body: group_id) |
| GET | `/api/v1/groups` | list my groups |
| GET | `/api/v1/groups/members?group_id=X` | list members |
| POST | `/api/v1/groups/messages/send` | send group message (body: group_id, content) |
| GET | `/api/v1/groups/messages/history?group_id=X` | get group message history |

### ODB Tables

- `im_groups` (group_id BIGSERIAL PK, name, creator_uid, created_at)
- `im_group_members` (id BIGSERIAL PK, group_id FK, user_uid, role, joined_at; unique on group_id+user_uid)
- `im_group_messages` (id BIGSERIAL PK, group_id, sender_uid, content, created_at)

### HTTP Status Mapping

| Error Code | HTTP Status |
|---|---|
| CREATOR_NOT_FOUND | 404 |
| GROUP_NOT_FOUND | 404 |
| USER_NOT_FOUND | 404 |
| ALREADY_MEMBER | 409 |
| NOT_MEMBER | 403 |
| OWNER_CANNOT_LEAVE | 403 |
| Other errors | 400 |

### Build Gating

- `MYCHAT_BUILD_GROUP_SERVICE` (default ON) gates the Group and GroupMessage ODB + service targets.
- `im_group_odb` requires ODB runtime.
- `im_group_message_odb` requires ODB runtime.
- `im_group_service` depends on `im::group_odb`, `im::group_message_odb`, and `im::user_service`.
- Gateway controllers are gated on `TARGET im::group_service` (for Group HTTP) and additionally `TARGET im::message_service` (for Group Message HTTP, which needs PushService).

## Test Suites

| Suite | Tests | Status |
|---|---|---|
| GroupServiceCoreTest | 17 | Pass |
| GatewayGroupHttpTest | 23 | Pass |
| GatewayGroupMessageHttpTest | 15 | Pass |
| Total (ODB-enabled) | 14/14 | Pass |

## Key Fixes

1. **Repository pass-by-reference**: `GroupRepository::create_group` took `Group` by value, so the auto-generated `group_id` was lost after `odb::persist`. Changed to `Group&`.

2. **Transaction nesting**: `GroupRepository::find_groups_by_user` opened a transaction then called `find_group_by_id` which opens its own transaction. Collected group_ids first, closed the first transaction, then called `find_group_by_id` for each.

3. **Gateway tests**: The `body["message"]` key does not exist in `HttpUtils::buildResponse` output (which uses `err_msg`). Group tests follow the Friend test pattern of checking only `res.status` for error cases.

4. **Group creation atomicity**: Group creation and owner membership insertion now happen in one repository transaction via `create_group_with_owner`, avoiding persisted groups without an owner if owner insertion fails.

5. **Group message validation and cleanup**: GroupMessageService validates group existence and sender membership before persisting. Friend/Group/GroupMessage tests clean relationship rows by resolving generated UIDs/group IDs from test account/name prefixes, avoiding orphan test data.

## New Files

| File | Purpose |
|---|---|
| `services/odb/group.hpp` | ODB Group + GroupMember model |
| `services/odb/generated/group-odb.*` | ODB-generated files |
| `services/odb/group_message.hpp` | ODB GroupMessage model |
| `services/odb/generated/group_message-odb.*` | ODB-generated files |
| `services/group/group_repository.{hpp,cpp}` | Group ODB CRUD |
| `services/group/group_service.{hpp,cpp}` | Group business logic |
| `services/group/group_message_repository.{hpp,cpp}` | GroupMessage ODB CRUD |
| `services/group/group_message_service.{hpp,cpp}` | GroupMessage business logic |
| `gateway/group_http_controller.{hpp,cpp}` | Group HTTP REST endpoints |
| `gateway/group_message_http_controller.{hpp,cpp}` | Group Message HTTP endpoints with fanout |
| `test/group/test_group_service.cpp` | 17 service-level tests, including GroupMessageService validation |
| `test/gateway_group/test_gateway_group_http.cpp` | 22 HTTP integration tests |
| `test/gateway_group_message/test_gateway_group_message_http.cpp` | 15 group message tests |

## Modified Files

| File | Change |
|---|---|
| `services/odb/CMakeLists.txt` | Added `im_group_odb`, `im_group_message_odb` targets |
| `services/group/CMakeLists.txt` | Added group_message source files |
| `services/CMakeLists.txt` | Added Group Service build gating |
| `gateway/CMakeLists.txt` | Added Group/GroupMessage HTTP controllers |
| `gateway/gateway_server/gateway_server.hpp` | Forward declarations + member vars |
| `gateway/gateway_server/gateway_server.cpp` | Route registration + init wiring |
| `Root CMakeLists.txt` | Added `MYCHAT_BUILD_GROUP_SERVICE` option |
| `test/CMakeLists.txt` | Added gateway_group, gateway_group_message dirs |
| `docs/devlog/current_progress.md` | Updated baseline and status |
| `docs/agent_context/roadmap.md` | Phase H marked complete |
| `docs/agent_context/todo.md` | Group MVP added to completed |

## Baseline

ODB-enabled build + test: 14/14 suites passing.

No-ODB build: 2/2 tests (Redis + AuthToken), skips Group targets cleanly.
