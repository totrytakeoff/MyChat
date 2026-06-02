---
type: roadmap
status: draft
updated_by: coder
---

# Roadmap

Aligned with `docs/architecture/service_mvp_roadmap.md`. Each phase builds a
testable MVP slice of one service before advancing to the next.

## Phase A: Foundation Baseline

Status: complete.

- Root CMake staging, vcpkg manifest, Docker Compose for Redis/PostgreSQL.
- Gateway startup from config, health endpoint, hiredis Redis wrapper migration.
- Focused Redis and Auth tests.

## Phase B: Gateway Service MVP Hardening

Status: complete.

- Removed duplicate gateway implementation.
- Hardened refresh-token and access-token revocation to per-token/per-JTI Redis keys.
- Auth token tests cover per-token storage, independent TTLs, wrong-device isolation.

## Phase C: PostgreSQL/ODB Foundation

Status: complete.

- ODB 2.5.0 compiler verified at `/usr/bin/odb`.
- `scripts/build_odb_runtime_2_5.sh` for reproducible runtime build from source.
- `im_user_odb` target, ODB-generated mapping files from `services/odb/user.hpp`.
- `ODBUserPersistenceTest` passes against Docker PostgreSQL.

## Phase D: User Service MVP

Status: complete.

- `im_user_service` with `PasswordHasher` (PBKDF2-HMAC-SHA256), `UserRepository` (ODB CRUD), `UserService` (register/login/profile).
- UUID v4 via `RAND_bytes`, deterministic test cleanup, no `DROP TABLE`.
- 5 focused test cases, all passing.

## Phase E: Gateway + User Service Integration

Status: complete.

- `UserHttpController` with `handle_register`, `handle_login`, `handle_profile`.
- Route registration seam (`register_user_http_routes_on_server`) before legacy catch-all handlers.
- 10 integration tests (9 controller + 1 HTTP route-layer).

## Phase F: Message Service MVP

Status: in progress (persistence core + HTTP integration complete; Gateway
         online delivery pending).

- ✅ Persistence core (task003): `services/message` target, ODB-backed message
  persistence, send one-to-one text, offline message pull, conversation history
  with `ORDER BY create_time`.
- ✅ Gateway HTTP integration (task004): authenticated HTTP endpoints for send,
  conversation history, and offline pull; token-based sender identity; auto-mark
  delivered on offline pull.
- [ ] WebSocket online delivery via ConnectionManager/Push path.
- Exit criteria: Message Service persistence tests pass; Gateway HTTP message
  API passes; Gateway can deliver message to online user; offline message is
  persisted and pullable.

## Phase G: Friend/Group/Push MVPs

Status: future work.

- Friend Service: friend requests, accept/reject, contact list.
- Group Service: group create/join/member list.
- Push Service: online fanout and deferred delivery.
