---
type: architecture_analysis
status: draft
updated_by: coder
review_required: true
---

# Architecture Analysis

## Current Shape

Phase 6 (Gateway-to-User HTTP Integration) is complete. The current runtime
topology is:

```
Client
  | HTTP: register/login/profile
  v
Gateway Service (port 8102 HTTP, 8101 WebSocket)
  |-- Auth token manager (JWT + Redis per-token/per-JTI storage)
  |-- Connection manager
  |-- Message parser / protobuf codec
  |-- Route registration: /api/v1/health, /api/v1/auth/{register,login,info}
  |
  | in-process calls (MVP shortcut)
  v
User Service Core (ODB/PostgreSQL)
  |-- PasswordHasher (PBKDF2-HMAC-SHA256)
  |-- UserRepository (ODB CRUD)
  |-- UserService (register/login/profile DTOs)
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

## Review Questions

1. Is the in-process Gateway-to-User shortcut acceptable until codec/gRPC is regenerated, or should service boundaries be enforced earlier?
2. Should the codec/gRPC regeneration be planned as a prerequisite for Message Service, or can Message Service follow the same direct-integration pattern?
3. Is the ODB-only persistence decision still correct, or should a lighter SQL option be available for developers who cannot build ODB 2.5.0?
4. Should `pgsql_conn.hpp` be fixed or deprecated before Friend/Group services start?
5. Is the single-connection Redis wrapper acceptable for the full MVP, or should a pool be added before Message/Push work begins?
