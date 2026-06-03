# MyChat Microservice MVP Architecture

## Current Goal

MyChat is being rebuilt as a staged C++ IM microservice project. The goal is not
to simplify the final architecture into a temporary single-process backend. The
goal is to make each service module reach an MVP state independently, then
compose those service MVPs into a usable IM system.

In this project, "MVP" means:

- one module has a clear responsibility boundary;
- it can be built and started in the development environment;
- its persistence/cache dependencies are explicit and Docker-backed;
- its core behavior is covered by focused tests;
- it exposes a stable enough contract for the next module to integrate.

This keeps the original microservice direction while controlling scope. We cut
unfinished feature breadth, not the architecture.

## Architecture Verdict

The original microservice direction is still the right long-term shape for this
project and should be preserved. The earlier problem was not "too much
architecture" by itself; it was too many half-finished modules without a stable
build, dependency baseline, or per-module test gate.

The corrected architecture decision is:

- Keep Gateway as an independent service MVP.
- Keep User, Message, Friend, Group, Push, and Codec as service boundaries.
- Keep PostgreSQL access on the ODB path instead of introducing a temporary
  hand-written SQL repository layer for user persistence.
- Keep Redis and PostgreSQL Docker-backed in development.
- Make every service independently buildable, testable, and documented before
  wiring broader integration.
- Use in-process shortcuts only for small internal helpers, not for collapsing
  service ownership boundaries.

## Kept Technology Choices

- C++20 for backend services and shared libraries.
- CMake for builds and staged target selection.
- vcpkg for third-party dependencies. Local default path:
  `/home/myself/pkgs/vcpkg`.
- Boost.Asio/Beast for network and WebSocket infrastructure.
- Protobuf as the canonical message contract.
- gRPC remains the service-to-service direction once generated service artifacts
  are cleaned and regenerated.
- Redis for access-token revocation, refresh-token metadata, online/session
  state, and cache-like IM data.
- PostgreSQL for durable user/message/friend/group data.
- ODB for PostgreSQL object mapping and database access wrappers.
- Docker Compose for local Redis/PostgreSQL, and later service orchestration.

## Runtime Topology

```text
Client
  | HTTP: register/login/refresh/pull/history
  | WebSocket: realtime IM traffic
  v
Gateway Service
  |-- Auth token manager
  |-- Connection manager
  |-- Message parser / protobuf codec
  |-- Router manager
  |
  | service calls (in-process for MVP)
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

Shared dependencies:
  Redis
  PostgreSQL
  Protobuf/gRPC contracts
```

> **MVP Decision (2026-06-03):** All backend services are linked as static libraries directly into the gateway binary. Inter-service calls are in-process function calls, not network RPC. This was chosen deliberately to ship working MVP features without blocking on gRPC/codec regeneration. Extraction into separate processes with gRPC boundaries is deferred until a concrete scalability need arises. The codec gRPC target (`MYCHAT_BUILD_CODEC_SERVICE`) remains OFF by default; stale generated codec files are not regenerated until service-split work begins.

## Service MVP Boundaries

`gateway`
: The client-facing service. It owns HTTP/WebSocket entry, request parsing,
  token verification, connection lifecycle, routing, and response shaping. It
  should not own durable user/message business rules.

`services/user`
: Owns account registration, login credential checks, user profile lookup, and
  user persistence through PostgreSQL/ODB.

`services/message`
: Owns direct-message persistence, offline-message state, history query, and
  delivery coordination with Gateway/Push.

`services/friend`
: Owns friendship relation, friend request lifecycle, and contact list query.

`services/group`
: Owns group metadata, membership, group messaging metadata, and permissions.

`services/push`
: Owns push fanout policy. It can use Gateway connection state for online
  delivery and Redis/PostgreSQL-backed queues for deferred delivery.

`services/codec`
: Owns service contract generation and codec/gRPC artifacts. The current codec
  artifacts are stale and must be regenerated before this service returns to the
  default build.

`common/network`
: Shared network primitives only. It must not know product business workflows.

`common/database`
: Shared Redis and PostgreSQL/ODB infrastructure. It owns connection, pooling,
  transaction helpers, and low-level storage primitives, not user/message
  workflows.

`common/proto`
: Canonical protobuf definitions and generated C++ files. Generated files are
  mechanical artifacts and must come from the project-approved `protoc`.

`common/utils`
: Shared logging, config, thread pool, signal handling, and small utilities.

## Development Rules

1. Preserve service boundaries. A module MVP is complete only when that module
   has its own build target, focused tests, and documented contract.
2. Use ODB for PostgreSQL-backed domain models once persistence work begins.
3. Redis/PostgreSQL must run through Docker Compose in development.
4. New service behavior requires focused tests under `test/`.
5. A feature is not complete until the relevant tests pass.
6. Hardcoded absolute paths are not allowed in production code.
7. Dependencies belong in `vcpkg.json` or documented system prerequisites.
8. Generated protobuf/ODB files must be regenerated through documented commands,
   not manually edited.
9. Old experimental tests can stay disabled only with a tracking note and a
   path to recovery.

## Current Runtime Entry

Development runtime currently starts Redis/PostgreSQL through Docker and starts
Gateway from one config file:

```bash
docker compose up -d redis postgres
/tmp/mychat-phase1/gateway/gateway_server --config config/dev.json
```

Default development ports:

- Gateway WebSocket: `8101`
- Gateway HTTP: `8102`
- Redis: `6379`
- PostgreSQL: `5432`

Gateway health check:

```text
GET /api/v1/health
```

## Current Status

Completed baseline:

- Root CMake can build staged targets.
- vcpkg manifest exists and points to the local toolchain root by default.
- Docker Redis/PostgreSQL services are available and healthy.
- Gateway can start from `config/dev.json`.
- Gateway health check works.
- Redis hiredis wrapper has focused tests.
- Auth token primitives have focused tests.
- Duplicate Gateway implementation has been collapsed into
  `gateway/gateway_server/gateway_server.cpp`.

Not complete yet:

- Gateway default business handlers are still incomplete.
- Gateway-to-service calls are not yet restored as real service boundaries.
- User Service is not yet a working service MVP.
- Message/Friend/Group/Push service MVPs are not started.
- `services/codec` contains stale generated gRPC/protobuf files and remains
  outside the default build.

## Known Technical Debt

- Refresh tokens currently use one Redis hash and need per-token Redis keys with
  independent TTLs.
- Access-token revocation currently uses one Redis set and needs per-token TTL
  cleanup.
- Redis wrapper is a minimal hiredis adapter, not a production connection pool.
- ODB 2.5.0 runtime must be built from source; not yet available in vcpkg.
- `pgsql_conn.hpp` RAII wrapper has string-ID and shared-ptr issues.
- Several old test suites still reflect `redis-plus-plus` or experimental
  Gateway APIs.
- Gateway handler registration includes temporary test-oriented paths and needs
  a real service contract cleanup.
- TLS certificate paths are development defaults; production certificate/secret
  handling is still open.
