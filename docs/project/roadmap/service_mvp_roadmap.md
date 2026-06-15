# Service MVP Roadmap

## Guiding Principle

MyChat will progress by completing one service MVP at a time. Each service MVP
must preserve the intended microservice boundary while keeping the feature set
small enough to finish and verify.

The sequence is vertical: finish a narrow, tested slice of one service, then
integrate it with the next service. Avoid expanding many services as empty
shells.

## Phase A: Foundation Baseline

Status: mostly complete.

Scope:

- Root CMake staging.
- vcpkg manifest.
- Docker Compose Redis/PostgreSQL.
- Protobuf regeneration target.
- Gateway startup from config.
- Redis wrapper migration to hiredis.
- Focused Redis/Auth tests.

Exit criteria:

- `gateway_server` builds.
- Redis/PostgreSQL containers are healthy.
- Gateway health endpoint returns `{"status":"ok"}`.
- `RedisHiredisTest` and `AuthTokenTest` pass.

## Phase B: Gateway Service MVP Hardening

Status: functionally complete; final stabilization in progress.

Scope:

- Keep Gateway as an independent service process.
- Remove temporary/test-only handler paths where they affect production flow.
- Fix refresh-token Redis model:
  - `refresh_token:<token>` stores token metadata;
  - `user:<uid>:refresh_tokens` indexes active tokens;
  - each refresh token key has independent TTL.
- Fix access-token revocation:
  - `revoked_access_token:<jti>` with TTL aligned to access token expiry.
- Keep `/api/v1/health` as a minimal operational endpoint.
- Keep token verification tests focused and deterministic.

Exit criteria:

- Gateway builds without known Gateway/Auth warnings.
- Auth token tests cover per-token Redis storage and revocation TTL behavior.
- Gateway health smoke still passes.
- Gateway runtime contract is documented.

## Phase C: PostgreSQL/ODB Foundation MVP

Status: complete.

Scope:

- Verify ODB compiler availability (2.5.0 installed at `/usr/bin/odb`).
- Build 2.5.0 runtime from source via `scripts/build_odb_runtime_2_5.sh`.
- CMake requires ODB 2.5.0 (fails at configure time if missing) through
  `MYCHAT_ODB_ROOT` or default `.odb/installed/`. No fallback to vcpkg 2.4.0.
- Bring `common/database/pgsql` and `services/odb` into the staged build.
- Generate ODB mapping files (`services/odb/generated/user-odb.*`) from
  `services/odb/user.hpp` using the 2.5.0 compiler.
- Add `ODBUserPersistenceTest` — uses `CREATE TABLE IF NOT EXISTS` and
  test-prefixed cleanup to avoid dropping the shared `im_users` table.
- `PgSqlConnection` RAII wrapper has pre-existing issues (string-ID support,
  raw-pointer return) — bypassed by using `odb::pgsql::database` directly
  in the persistence test.

Exit criteria (all met):

- `scripts/build_odb_runtime_2_5.sh` installs the 2.5.0 runtime reproducibly.
- CMake fails at configure time if ODB 2.5.0 runtime is not found.
- `im_user_odb` builds with `MYCHAT_BUILD_PGSQL_ODB=ON`.
- `ODBUserPersistenceTest` passes against Docker PostgreSQL.
- Default no-ODB config still clean (2/2 tests pass).
- ODB generation command is documented in CMakeLists.txt error message.

## Phase D: User Service MVP

Status: complete.

Scope:

- `services/user/` CMake target (`im_user_service`), deterministic sources,
  gated on `MYCHAT_BUILD_PGSQL_ODB=ON` + `TARGET im::user_odb`.
- `PasswordHasher` — PBKDF2-HMAC-SHA256 via OpenSSL, configurable iterations,
  constant-time comparison, self-describing format `pbkdf2_sha256$iter$salt$hash`.
- `UserRepository` — ODB-backed CRUD for `im::service::user::User`.
- `UserService` — register/login/profile with C++ DTO structs (no
  password_hash in profile responses).
- UUID v4 UID generation via OpenSSL `RAND_bytes` (no libuuid dependency).
- Register rejects empty account/password, duplicate accounts.
- Login validates credentials, updates `last_login`.
- Profile lookup by UID or account.
- `UserServiceCoreTest` — 5 focused test cases, deterministic cleanup by
  `DELETE WHERE account LIKE 'task4-test-%'`, no `DROP TABLE`.
- Bypasses `PgSqlConnection` by using `odb::pgsql::database` directly.

Exit criteria (all met):

- `im_user_service` builds with `MYCHAT_BUILD_SERVICES=ON MYCHAT_BUILD_PGSQL_ODB=ON`.
- User Service tests pass against Docker PostgreSQL (100% of 4 ODB-enabled tests).
- Register/login/profile workflows pass at service level.
- Password hashing is tested (hash differs from plaintext, format verified).
- Default no-ODB config still clean (2/2 tests pass).
- Service API contract documented in
  `docs/history/devlog/phase4_user_service_core.md`.

## Phase E: Gateway + User Service Integration

Status: complete.

Scope:

- Wire Gateway auth HTTP routes to User Service:
  - `/api/v1/auth/register`;
  - `/api/v1/auth/login`;
  - `/api/v1/auth/refresh`;
  - `/api/v1/auth/logout`.
- Gateway remains responsible for token issuance/verification, unless the
  service contract deliberately moves token issuance into User Service.
- User Service remains responsible for durable account data.

Exit criteria:

- Gateway endpoint tests pass.
- Login returns access/refresh tokens.
- User profile lookup is available through `GET /api/v1/auth/info`.
- Refresh/logout remain auth-token manager capabilities but are not the current
  primary Gateway HTTP contract.

## Phase F: Message Service MVP

Status: complete for MVP behavior; local stabilization passed.

Scope:

- Create `services/message` target.
- Store direct messages through PostgreSQL/ODB.
- Support:
  - send one-to-one text message;
  - save offline message;
  - query history;
  - mark delivered/read if included in MVP contract.
- Integrate Gateway online delivery path through ConnectionManager/Push path.

Exit criteria:

- Message Service tests pass.
- Gateway can send a message to an online user.
- Offline message can be persisted and later pulled.
- Push fanout, `PushRuntime`, remote Push gRPC, standalone `push_server`, and
  Gateway callback delivery have focused tests and process-level smoke
  coverage.
- Remaining Phase F-adjacent work is operational hardening rather than new
  user-facing message features.

## Phase G: Friend/Group/Push MVPs

Status: complete for MVP behavior; local stabilization passed.

Scope:

- Friend Service: friend requests, accept/reject, contact list.
- Group Service: group create/join/member list/basic group message metadata.
- Push Service: online fanout and deferred delivery policy.

Exit criteria:

- Friend and Group have their own tests and documented Gateway API.
- Push has service-owned fanout/runtime code, a gRPC boundary, standalone
  process slice, Gateway delivery callback path, and process-level remote
  smokes. It is not yet treated as a fully productionized independent service.

## Phase H: Distributed Service Boundary Stabilization

Status: locally stable; release hardening remains.

Scope:

- Keep default builds local/gRPC-off for fast development.
- Keep explicit gRPC builds for User, Message, Friend, Group, and Push.
- Verify Gateway local and remote facades preserve the same HTTP/WS external
  contracts.
- Keep schema migration and Redis/PostgreSQL startup explicit through local
  scripts.
- Make heavy ODB/gRPC regression repeatable and document serial test rules.

Exit criteria:

- `scripts/ci/checks.sh` passes.
- `scripts/ci/default_regression.sh` passes.
- `scripts/ci/remote_push_odb.sh` passes serially as the heavy remote-services
  ODB/gRPC regression.
- Runtime/config docs clearly distinguish default local mode from explicit
  remote service mode.

Current status: exit criteria are met locally. Remaining work is release
hardening: hosted CI, production secrets/TLS, packaging, and stronger ODB test
data isolation for future parallel CI.

## Phase I: Web Validation Client

Status: first Web client slice implemented and runtime-validated against the
remote-all local stack.

Scope:

- Build a Web desktop-style IM validation client under `clients/web`.
- Reference QQ and Telegram Desktop interaction patterns:
  left rail, conversation list, main chat surface, and optional debug drawer.
- Keep the first client focused on backend validation, not final product polish.
- Validate the current Gateway HTTP and WebSocket contracts through real user
  workflows:
  - register/login/profile;
  - direct message send/history/offline pull;
  - WebSocket direct-message send and online push receive;
  - friend request/respond/list/pending;
  - group create/join/leave/list/member list;
  - group message send/history and online group fanout.
- Keep backend local/remote mode transparent to the UI. The Web client talks to
  Gateway only; Gateway decides local facades vs. remote gRPC services.
- Document and preserve the API adapter shape so future Qt and Electron
  clients can reuse the same interaction model.

Reference design document:

```text
docs/project/architecture/web_validation_client_plan.md
```

Exit criteria:

- `clients/web` starts locally with configurable HTTP/WS endpoints.
- A developer can register or login two users and keep separate sessions.
- Direct HTTP message send, history, and offline pull are usable.
- WebSocket realtime send/ack/push can be verified with two browser sessions.
- Friend and group MVP flows are usable from the UI.
- A debug drawer shows endpoint settings, current user, token state,
  connection state, last HTTP responses, and recent WebSocket events.

Current status: exit criteria are met locally. Playwright two-context
validation on 2026-06-08 covered register/login, per-tab device IDs, friend
search/request/accept, contact/group tab switching, direct online push without
manual refresh, group create/join, group message history, and group online push
routed by push metadata. Remaining work is enhancement-level: add richer
display metadata such as sender nickname in push payloads, and decide later
whether browser protobuf bindings should replace the current minimal wire
adapter.

## Phase J: Interview and Portfolio Review

Status: current active phase.

Scope:

- Treat the current system as a complete practical MVP, not an endlessly
  unfinished product.
- Build documentation that explains the project as an interview-ready C++
  distributed IM backend:
  - architecture overview;
  - module responsibilities;
  - auth/profile flow;
  - direct-message flow;
  - friend flow;
  - group-message flow;
  - Push/fanout flow;
  - local vs. remote service mode;
  - persistence, Redis, schema migration, and test strategy.
- Guide manual validation through the Web client while tying each action back
  to backend interfaces and service boundaries.
- Prepare a later multi-machine validation pass for the remote-all topology.
- Separate MVP-complete capabilities from future production extensions so the
  resume and interview story stay accurate.

Exit criteria:

- A developer can explain the system from Gateway entrypoint to service,
  storage, push, and Web client behavior.
- A manual checklist can validate the current MVP end to end.
- A multi-machine runbook documents required config/ports/startup order and
  Push callback topology.
- Common interview questions and tradeoff answers are recorded per module.
- Resume bullets describe the implemented system honestly and concretely.

Reference plan:

```text
docs/project/architecture/interview_review_plan.md
```

## Build/Test Gate

Before moving from one phase to the next:

```bash
docker compose up -d redis postgres
cmake -S . -B /tmp/mychat-dev \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-dev -j2
ctest --test-dir /tmp/mychat-dev --output-on-failure
```

During transition, old suites may remain disabled, but every newly completed
module must have a focused `ctest -R ...` command recorded in the devlog.
