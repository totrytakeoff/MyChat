---
type: project_context
status: draft
updated_by: coder
---

# Project Context

## Purpose

MyChat is a C++ instant messaging system being rebuilt as a staged microservice
project. The goal is to make each service module (Gateway, User, Message,
Friend, Group, Push, Codec) reach an MVP state independently, then compose
those service MVPs into a usable IM system. Each MVP preserves its intended
microservice boundary while keeping the feature set small enough to finish and
verify.

## Tech Stack

- Language/runtime: C++20, g++
- Frameworks: Boost.Asio/Beast (network/WebSocket), Protobuf (message contract), gRPC (service-to-service direction)
- Build system: CMake with staged options, vcpkg for third-party dependencies
- Test system: Google Test (GTest) via CTest
- Storage/services: Redis (hiredis wrapper for token/session state), PostgreSQL 16 (ODB 2.5.0 for object persistence), Docker Compose for local dev

## Repository Map

- `common/` - Shared components: network primitives (WebSocket server/session, protobuf codec), database wrappers (Redis hiredis, PostgreSQL ODB), proto definitions, utilities (logging, config, signal handling)
- `services/` - Microservice implementations: user (ODB-backed MVP with register/login/profile), message (ODB-backed persistence core with direct text send/history/offline pull), friend/group/push (future), codec (stale, gated OFF by default)
- `gateway/` - Client-facing service: HTTP/WebSocket entry, auth token management, connection management, message routing, route registration
- `test/` - Focused test modules: auth, db, odb, user, gateway_user, message, legacy (gated)
- `scripts/` - Build helper scripts (e.g., `build_odb_runtime_2_5.sh`)
- `config/` - Development configuration seed (`dev.json`)
- `docs/` - Architecture docs, devlog per phase, agent context

## Build And Test Commands

- Install deps: `vcpkg install` (or rely on CMake manifest mode with `-DVCPKG_MANIFEST_FEATURES=pgsql-odb` for ODB)
- Configure (ODB): `cmake -S . -B /tmp/mychat-build -DVCPKG_MANIFEST_FEATURES=pgsql-odb -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON -DMYCHAT_BUILD_SERVICES=ON -DMYCHAT_BUILD_PGSQL_ODB=ON -DCMAKE_BUILD_TYPE=Debug`
- Configure (no-ODB): same without `PGSQL_ODB` and `VCPKG_MANIFEST_FEATURES`
- Build: `cmake --build /tmp/mychat-build -j2`
- Test: `ctest --test-dir /tmp/mychat-build --output-on-failure`
- Docker: `docker compose up -d redis postgres`
- Gateway: `/tmp/mychat-build/gateway/gateway_server --config config/dev.json`

## Current Architecture Facts

- Gateway is a standalone process with HTTP (port 8102) and WebSocket (port 8101) endpoints, health check at `GET /api/v1/health`.
- Auth uses JWT-based dual token system (access + refresh) with Redis-backed per-token storage and per-JTI revocation keys.
- User Service (Phase 4) provides register/login/profile via ODB-backed PostgreSQL persistence, using PBKDF2-HMAC-SHA256 password hashing.
- Gateway-to-User integration (Phase 6) wires three HTTP routes: `POST /api/v1/auth/register`, `POST /api/v1/auth/login`, `GET /api/v1/auth/info`.
- ODB 2.5.0 compiler is at `/usr/bin/odb`; runtime must be built from source via `scripts/build_odb_runtime_2_5.sh`.
- Message Service persistence core (Task 003) provides ODB-backed `im_messages` storage, validated one-to-one text send, chronological conversation history, offline pull for undelivered messages, and delivered/read marking. It is not yet wired into Gateway HTTP/WebSocket delivery.
- CMake staged build options: `MYCHAT_BUILD_TESTS`, `MYCHAT_BUILD_GATEWAY`, `MYCHAT_BUILD_SERVICES`, `MYCHAT_BUILD_USER_SERVICE`, `MYCHAT_BUILD_MESSAGE_SERVICE`, `MYCHAT_BUILD_CODEC_SERVICE`, `MYCHAT_BUILD_PGSQL_ODB`, `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`, `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.
- Active test count: 6 with ODB enabled (RedisHiredisTest, ODBUserPersistenceTest, UserServiceCoreTest, GatewayUserHttpTest, MessageServiceCoreTest, AuthTokenTest); no-ODB builds skip Message Service targets cleanly and keep the no-ODB baseline green.

## Constraints

- ODB 2.5.0 runtime must be built from source; CMake fails at configure time if missing.
- vcpkg root configured at `/home/myself/pkgs/vcpkg`.
- Docker Redis and PostgreSQL required for most tests.
- `services/codec` contains stale generated gRPC/protobuf files; gated OFF by default.
- Message Service targets require ODB and are skipped when `im::message_odb` is unavailable.
- Legacy test suites (router, network, utils) have pre-existing failures; gated behind `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.

## Known Risks

- ODB 2.5.0 runtime build is a manual step not yet covered by CI.
- `pgsql_conn.hpp` RAII wrapper has pre-existing string-ID and raw-pointer issues; User Service bypasses it by using `odb::pgsql::database` directly.
- Redis wrapper is single-connection and mutex-serialized — adequate for correctness tests, not for performance.
- Stale codec/gRPC generated files may cause confusion if accidentally regenerated with mismatched protoc versions.
- Legacy tests (SignalHandlerTest, RouterManagerTests) have pre-existing failures if re-enabled.
- No schema migration framework for PostgreSQL yet.
- Full Phase F is not complete: Gateway online delivery, Push fanout, and service-call strategy remain future work.
- `SendRequest::msg_type` is caller-supplied even though the current API method is named `send_text_message`; defaulting to `MessageType::TEXT` is a small future cleanup.
- TLS certificate paths are development defaults; production certificate/secret handling is still open.
