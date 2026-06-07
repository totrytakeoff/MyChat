---
type: architecture_analysis
status: draft
updated_by: coder
review_required: true
---

# Architecture Analysis

## Current Shape

Phase E (Gateway-to-User HTTP Integration) is complete. Task 003 added the
Message Service persistence core, and Task 004 exposed it through authenticated
Gateway HTTP routes. The current runtime topology is:

```
Client
  | HTTP: register/login/profile
  v
Gateway Service (port 8102 HTTP, 8101 WebSocket)
  |-- Auth token manager (JWT + Redis per-token/per-JTI storage)
  |-- Connection manager
  |-- Message parser / protobuf codec
  |-- Route registration: /api/v1/health,
  |   /api/v1/auth/{register,login,info},
  |   /api/v1/messages/{send,history,offline}
  |
  | in-process calls (MVP shortcut)
  v
User Service Core (ODB/PostgreSQL)
  |-- PasswordHasher (PBKDF2-HMAC-SHA256)
  |-- UserRepository (ODB CRUD)
  |-- UserService (register/login/profile DTOs)

Message Service Core (ODB/PostgreSQL; Gateway HTTP-wired)
  |-- MessageRepository (ODB CRUD)
  |-- MessageService (validated direct text send/history/offline pull)
  |-- im_messages table and ODB-generated mapping files
```

Shared dependencies: Docker Redis (port 6379), Docker PostgreSQL (port 5432).

## Target Shape

The long-term microservice topology (from `docs/architecture/mvp_architecture.md`):

```
Client (HTTP + WebSocket)
  v
Gateway Service
  | service calls (gRPC direction)
  v
+----------------+     +------------------+
| User Service   |     | Message Service  |
| PostgreSQL/ODB |     | PostgreSQL/ODB   |
+----------------+     +------------------+
        |                       |
        v                       v
+----------------+     +------------------+
| Friend Service |     | Push Service     |
| PostgreSQL/ODB |     | Redis/WebSocket  |
+----------------+     +------------------+

Shared dependencies: Redis, PostgreSQL, Protobuf/gRPC contracts
```

Service boundaries are preserved. Gateway defaults to in-process User calls
through `LocalUserClient`, but the User gRPC service contract, standalone
`user_server`, and Gateway `RemoteUserClient` path now exist. User HTTP routes
can be switched to cross-process calls with `user.mode=remote` without changing
the external auth/profile API.

## Key Decisions

- Decision: Preserve microservice architecture (do not collapse into single process).
  Rationale: The original multi-service direction is correct; the earlier problem was too many half-finished modules without stable build, dependency baseline, or per-module test gates.

- Decision: Use ODB for PostgreSQL persistence.
  Rationale: ODB provides compile-time type-safe SQL mapping. A hand-written SQL repository layer was rejected as a throwaway intermediate step unless ODB proves unavailable.

- Decision: Stage builds with CMake options, not monolithic all-or-nothing.
  Rationale: Lets developers work on Gateway without installing PostgreSQL/ODB, and enables incremental MVP completion.

- Decision: Use hiredis (C) instead of redis-plus-plus (C++).
  Rationale: redis-plus-plus had compatibility issues with the project's C++20/Boost setup; hiredis is smaller, well-tested, and sufficient for MVP needs.

- Decision: Gateway-only route registration, no codec gating on auth endpoints.
  Rationale: Auth endpoints were wired directly in Gateway HTTP handlers (Phase 6) rather than waiting for gRPC codec regeneration, keeping the integration MVP simple.

- Decision: Add standalone `user_server` before changing Gateway auth/profile
  handlers.
  Rationale: The generated User gRPC contract and `UserGrpcService` adapter are
  now process-tested independently, so the next change can focus only on
  Gateway client-side selection and error mapping.

- Decision: Put a small `UserClient` facade between User HTTP and UserService.
  Rationale: This preserves the existing controller and HTTP response mapping
  while allowing `GatewayServer` to choose local `UserService` or remote
  `im.user.UserService::Stub` based on config.

- Decision: Build Message Service persistence before Gateway delivery.
  Rationale: Phase F is split so ODB-backed message storage, validation, chronological history, offline pull, and delivered/read marking are independently tested before HTTP/WebSocket delivery and Push fanout are added.

- Decision: Use the same in-process Gateway-to-service shortcut for Message HTTP integration.
  Rationale: Task 004 exposes send/history/offline through Gateway HTTP without waiting for codec/gRPC regeneration, matching the Gateway-to-User MVP pattern while preserving future service-call boundaries.

- Decision: Trust authenticated token UID as the Message HTTP actor.
  Rationale: Gateway ignores client-supplied sender identity for message send/offline actor behavior and derives the sender from the verified Bearer access token.

## Risks And Tradeoffs

- Risk: ODB 2.5.0 runtime build is manual and not CI-tracked.
  Mitigation: `scripts/build_odb_runtime_2_5.sh` provides a reproducible build; CMake fails fast at configure time if runtime is missing.

- Risk: Broadly migrating repositories onto the shared `PgSqlConnection` wrapper could churn stable service code.
  Mitigation: `PgSqlConnection` is repaired for current string-ID ODB usage and covered by `PgSqlConnectionTest`, but User/Message/Friend/Group repositories should remain on direct `odb::pgsql::database` unless a separate boundary plan chooses otherwise.

- Risk: Redis pool sizing still lacks quantified load/benchmark data.
  Mitigation: RedisManager now uses a RAII connection pool keyed by `RedisConfig::pool_size`; pool wait timeout is configurable through `redis.pool_wait_timeout`; focused tests cover pool stats, exhaustion timeout, concurrent borrowers, reinitialize boundaries, Auth token concurrency, ConnectionManager session metadata reads, and no-live-WS PushService lookup semantics. Full Gateway remote Push smoke also asserts the pool returns idle after real TLS WebSocket registration, direct remote Push delivery, HTTP group remote Push fanout, and a concurrent direct-send slice with 6 sender/receiver pairs sharing a pool of 4 Redis connections. RedisClient reconnects and retries once after hiredis reports a connection-level empty reply; focused Redis coverage kills a live Redis client connection and verifies the next command succeeds.

- Risk: Codec service is still only a placeholder outside the active runtime.
  Mitigation: Active generated outputs live under `common/proto`; do not
  reintroduce generated files under `services/codec`.

- Risk: User, Message, and Push now have pinned remote paths, but Friend and
  Group still lack formal gRPC server/client boundaries and other services
  still consume local service objects in-process.
  Mitigation: `common/proto/user.proto` now defines `im.user.UserService`
  Register/Login/GetUserInfo, `generate_user_grpc` produces canonical
  `common/proto/user.grpc.pb.*`, `services/user/UserGrpcService` maps generated
  gRPC calls to the ODB-backed UserService, `services/user/user_server` hosts
  the adapter as a standalone process, and Gateway User HTTP can use
  `RemoteUserClient` when `user.mode=remote`. `common/proto/message.proto`
  now defines `im.message.MessageService`, `generate_message_grpc` produces
  canonical `common/proto/message.grpc.pb.*`, and
  `services/message/MessageGrpcService` maps generated gRPC calls to the
  ODB-backed MessageService. `services/message/message_server` hosts the
  adapter as a standalone process, and Gateway Message HTTP/WS plus Push
  delivered marking can use `RemoteMessageClient` when `message.mode=remote`.
  Keep Friend/Group gRPC contracts, Gateway remote Friend/Group facades, and
  service-to-service User lookups as explicit follow-up slices.

- Risk: PostgreSQL migration startup policy is not decided yet.
  Mitigation: `db/migrations/001_core_schema.sql` and
  `scripts/db/migrate_postgres.sh` provide a non-destructive baseline with
  version/checksum tracking. Service, Gateway, and remote Push smoke tests use
  `test/support/postgres_schema.*`; decide later whether dev runtime startup
  should run migrations or require an explicit pre-start migration step.

- Risk: Full Message Service MVP is still incomplete.
  Mitigation: Roadmap and todo keep Phase F in progress; WebSocket send/ack, online delivery through `ConnectionManager`, and Push fanout remain explicit next work.

- Risk: `SendRequest::msg_type` is caller-supplied while the service method is named `send_text_message`.
  Mitigation: Leave behavior as reviewed for Task 003; consider defaulting to `MessageType::TEXT` in a later API cleanup.

- Risk: Auth token expiry tests have a timing-sensitive transient failure mode.
  Mitigation: Treat the observed `AuthTokenTest.IndependentExpiryPerRefreshToken` retry pass as pre-existing infrastructure risk; avoid attributing it to Message HTTP integration unless reproduced deterministically.

## Review Questions

1. Should Friend gRPC be split into request/respond/list/pending methods that
   mirror `FriendService`, or should it expose a coarser command-style API?
2. Should Friend/Group gRPC boundaries call remote User directly for existence
   checks, or should they keep receiving a local UserService-compatible facade
   until all service processes are split?
3. Is the ODB-only persistence decision still correct, or should a lighter SQL
   option be available for developers who cannot build ODB 2.5.0?
4. Should any future repository adopt `PgSqlConnection`, or should service
   repositories keep the direct `odb::pgsql::database` pattern?
