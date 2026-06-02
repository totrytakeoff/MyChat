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

Service boundaries are preserved; current in-process calls from Gateway to User
Service are an MVP shortcut that will become real service calls (gRPC) once the
codec/gRPC artifacts are regenerated.

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

- Decision: Build Message Service persistence before Gateway delivery.
  Rationale: Phase F is split so ODB-backed message storage, validation, chronological history, offline pull, and delivered/read marking are independently tested before HTTP/WebSocket delivery and Push fanout are added.

- Decision: Use the same in-process Gateway-to-service shortcut for Message HTTP integration.
  Rationale: Task 004 exposes send/history/offline through Gateway HTTP without waiting for codec/gRPC regeneration, matching the Gateway-to-User MVP pattern while preserving future service-call boundaries.

- Decision: Trust authenticated token UID as the Message HTTP actor.
  Rationale: Gateway ignores client-supplied sender identity for message send/offline actor behavior and derives the sender from the verified Bearer access token.

## Risks And Tradeoffs

- Risk: ODB 2.5.0 runtime build is manual and not CI-tracked.
  Mitigation: `scripts/build_odb_runtime_2_5.sh` provides a reproducible build; CMake fails fast at configure time if runtime is missing.

- Risk: `pgsql_conn.hpp` RAII wrapper has unresolved template issues (string-ID `std::to_string`, raw-pointer vs shared_ptr).
  Mitigation: User Service bypasses it by using `odb::pgsql::database` directly. Fix when the wrapper becomes a blocker.

- Risk: Single-connection Redis wrapper limits throughput and provides no failover.
  Mitigation: Sufficient for correctness testing; a connection pool is deferred until load requirements emerge.

- Risk: Stale codec/gRPC generated files are outside the default build but present in the tree.
  Mitigation: Gated behind `MYCHAT_BUILD_CODEC_SERVICE=OFF`. Must be regenerated before Message Service integration.

- Risk: No schema migration framework for PostgreSQL.
  Mitigation: Currently handled by `CREATE TABLE IF NOT EXISTS` + test-prefixed `DELETE`. Acceptable during MVP.

- Risk: Full Message Service MVP is still incomplete.
  Mitigation: Roadmap and todo keep Phase F in progress; WebSocket send/ack, online delivery through `ConnectionManager`, and Push fanout remain explicit next work.

- Risk: `SendRequest::msg_type` is caller-supplied while the service method is named `send_text_message`.
  Mitigation: Leave behavior as reviewed for Task 003; consider defaulting to `MessageType::TEXT` in a later API cleanup.

- Risk: Auth token expiry tests have a timing-sensitive transient failure mode.
  Mitigation: Treat the observed `AuthTokenTest.IndependentExpiryPerRefreshToken` retry pass as pre-existing infrastructure risk; avoid attributing it to Message HTTP integration unless reproduced deterministically.

## Review Questions

1. Is the in-process Gateway-to-User shortcut acceptable until codec/gRPC is regenerated, or should service boundaries be enforced earlier?
2. Should the remaining Gateway-to-Message WebSocket/online delivery work continue the direct in-process pattern first, or should codec/gRPC regeneration happen before that delivery work?
3. Is the ODB-only persistence decision still correct, or should a lighter SQL option be available for developers who cannot build ODB 2.5.0?
4. Should `pgsql_conn.hpp` be fixed or deprecated before Friend/Group services start?
5. Is the single-connection Redis wrapper acceptable for the full MVP, or should a pool be added before Message/Push work begins?
