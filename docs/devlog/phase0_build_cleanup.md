# Phase 0 Build Cleanup

## Purpose

The project had useful common/network/protocol components, but the repository
could not be built reliably from a clean CMake configure. Phase 0 makes the
foundation buildable before feature work continues.

## Findings

- The root build required every dependency even when only common libraries were
  being changed.
- The previous CMake configuration searched unused Boost components such as
  filesystem, coroutine, context, and thread.
- Several subdirectory CMake files hardcoded `/home/myself/vcpkg`.
- The active local vcpkg root is `/home/myself/pkgs/vcpkg`.
- Redis/JWT/httplib/libuuid dependencies were not available from the system
  package set; they must come from vcpkg.
- Submitted protobuf generated files were produced by a different Protobuf
  version than the active compiler/runtime.
- `services/codec` contains old duplicated generated protobuf/grpc files and is
  disabled for now.
- `test/utils/CMakeLists.txt` registered `test_utils_integration` without
  defining that executable.

## Changes Made

- Added `vcpkg.json` to declare third-party dependencies.
- Added `docker-compose.yml` for Redis and PostgreSQL development services.
- Added `config/dev.json` as the development service configuration seed.
- Added staged CMake options:
  - `MYCHAT_BUILD_TESTS`
  - `MYCHAT_BUILD_GATEWAY`
  - `MYCHAT_BUILD_SERVICES`
- Changed the default local vcpkg root to `/home/myself/pkgs/vcpkg`.
- Replaced the previous `redis-plus-plus` dependency with a thin hiredis-backed
  Redis wrapper for the MVP. The wrapper currently covers hash/set/delete/expire
  operations used by auth and connection management.
- Made services disabled by default until old codec generated files are cleaned.
- Removed stale test target registration in `test/utils/CMakeLists.txt`.
- Disabled the old Redis test target because it still depends on
  `redis-plus-plus` APIs and must be rewritten for the new wrapper.
- Regenerated `common/proto/*.pb.cc` and `common/proto/*.pb.h` with vcpkg
  Protobuf `protoc-29.3.0`.
- Added a `generate_proto` CMake target that uses the configured Protobuf
  compiler.
- Updated `ProtobufCodec` for newer Protobuf `GetTypeName()` returning
  `std::string_view`.
- Added a real `gateway/main.cpp` entry point that reads `config/dev.json`,
  initializes Redis, starts the gateway, and handles SIGINT/SIGTERM shutdown.
- Fixed `GatewayServer` so failed initialization cannot leave a half-built
  object that later crashes in `start()`.
- Fixed a shutdown crash caused by process-global service lifetime interacting
  with static logger teardown.
- Fixed `LogManager::SetLogLevel()` to protect shared logger state with the same
  mutex used by other logging APIs.
- Fixed the PostgreSQL development schema so Docker initialization succeeds:
  the user table is now named `users` instead of the reserved keyword `user`,
  and role creation uses repeatable `DO` blocks.

## Verification

This command now configures and builds the common libraries with the local vcpkg
toolchain:

```bash
cmake -S . -B /tmp/mychat-cmake-vcpkg \
  -DMYCHAT_BUILD_TESTS=OFF \
  -DMYCHAT_BUILD_GATEWAY=OFF \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-cmake-vcpkg -j2
```

Regenerate protocol files with:

```bash
cmake --build /tmp/mychat-cmake-vcpkg --target generate_proto
```

Verified targets:

- `im_utils`
- `im_network`
- `im_proto`
- `im_database`
- `im_gateway_auth`
- `im_gateway_core`
- `gateway_server`

Docker development services were verified with:

```bash
docker compose up -d redis postgres
docker compose ps
```

Both `mychat-redis` and `mychat-postgres` reached `healthy`.

Gateway smoke test:

```bash
/tmp/mychat-cmake-gateway/gateway/gateway_server --config config/dev.json
curl http://127.0.0.1:8102/api/v1/health
```

The health endpoint returned `{"status": "ok"}` and SIGINT shutdown exited with
code `0`.

## Open Issues

- The hiredis wrapper is intentionally narrow and currently serializes access
  through one connection. Replace it with a real small pool before load testing.
- Redis tests must be rewritten around the new `RedisClient` API.
- `services/codec` should either be removed or regenerated properly with gRPC
  dependencies.
- Network code still emits warnings for unused callback parameters and member
  initialization order.
- Gateway default message handler registration is incomplete and must be
  redesigned during the Auth/Message MVP work.
- WebSocket TLS currently uses development certificate paths from
  `config/dev.json`; production certificate and secret handling is still open.
