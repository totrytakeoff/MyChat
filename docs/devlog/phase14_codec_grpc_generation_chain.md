# Phase 14: Codec/gRPC Generation Chain Cleanup

Date: 2026-06-04

## Purpose

Clean up the codec/gRPC generation chain before starting a standalone Push
Service. This phase makes protobuf and codec gRPC generation deterministic from
canonical `common/proto` inputs. It does not implement a running CodecService or
change Gateway/service behavior.

## Repository Snapshot

Current baseline snapshot was committed before this plan:

```text
e076fe6 codgent: complete fanout friend group snapshot
```

That commit captures the verified FanoutPolicy, Friend Service, Group Service,
Group Message HTTP, tests, generated ODB files, and handoff documentation.

Verified before this plan:

```text
ODB-enabled baseline: 14/14 tests passed.
No-ODB baseline: 2/2 tests passed.
Codec explicit-enable smoke build: im_codec_service builds when gRPC is
available.
```

## Implemented State

Canonical protobuf source directory:

```text
common/proto/
```

Current proto files:

- `base.proto`
- `command.proto`
- `user.proto`
- `friend.proto`
- `group.proto`
- `message.proto`
- `push.proto`
- `codec_service.proto`

Current generated protobuf files:

- `common/proto/*.pb.cc`
- `common/proto/*.pb.h`
- `common/proto/codec_service.grpc.pb.cc`
- `common/proto/codec_service.grpc.pb.h`

Historical duplicated codec generated files still exist but are no longer in
the active build path:

- `services/codec/base.pb.*`
- `services/codec/codec_service.pb.*`
- `services/codec/codec_service.grpc.pb.*`

Build wiring after cleanup:

- Root CMake defines explicit generated output lists.
- `generate_common_proto` regenerates protobuf C++ files in `common/proto`.
- `generate_codec_grpc` regenerates `codec_service.grpc.pb.*` in
  `common/proto` when `grpc_cpp_plugin` is available.
- Aggregate `generate_proto` depends on `generate_common_proto` and, when the
  gRPC plugin is available, `generate_codec_grpc`.
- Default builds with `MYCHAT_BUILD_CODEC_SERVICE=OFF` do not require gRPC and
  do not fail if `generate_codec_grpc` is unavailable.
- `im::proto` now compiles `codec_service.pb.cc` in addition to the existing
  base/command/user/friend/group/message/push generated sources.
- `im_codec_service` is built directly in `services/CMakeLists.txt` when
  `MYCHAT_BUILD_CODEC_SERVICE=ON`.
- `im_codec_service` now consumes `common/proto/codec_service.grpc.pb.cc` and
  links `im::proto`; it no longer compiles `services/codec/base.pb.cc`.
- `services/codec/CMakeLists.txt` is effectively unused.
- `services/codec/main.cpp` is a placeholder and does not implement a running
  codec service.

Toolchain facts observed locally:

- Shell default `protoc` is `/usr/bin/protoc`, version `libprotoc 34.1`.
- CMake selects vcpkg Protobuf compiler:
  `/home/myself/pkgs/vcpkg/installed/x64-linux/tools/protobuf/protoc-29.3.0`.
- Checked-in generated `.pb.*` files show Protobuf C++ Version `5.29.3`.
- gRPC plugin exists locally:
  `/home/myself/pkgs/vcpkg/installed/x64-linux/tools/grpc/grpc_cpp_plugin`.
- gRPC CMake config exists locally under the vcpkg installation.
- Optional vcpkg feature `codec-grpc` now declares the `grpc` dependency.

Codec explicit-enable smoke command:

```bash
cmake -S . -B /tmp/mychat-build-codec \
  -DVCPKG_MANIFEST_FEATURES=codec-grpc \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_CODEC_SERVICE=ON \
  -DMYCHAT_BUILD_GATEWAY=OFF \
  -DMYCHAT_BUILD_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-codec --target generate_proto -j2
cmake --build /tmp/mychat-build-codec --target im_codec_service -j2
```

Result:

```text
generate_proto built successfully.
im_codec_service built successfully.
```

## Decisions

Use `common/proto` as the single canonical proto source tree.

Generated protobuf/gRPC outputs remain checked in under `common/proto` for now.
Moving to an out-of-tree generated directory is intentionally deferred because
it would touch many include paths.

Do not start standalone Push Service work until this generation chain is clean
enough that future service contracts can be regenerated without manual file
copying.

## Task 009 Result

Goal:

```text
Create a deterministic CMake-driven protobuf/gRPC generation chain that uses
common/proto as the only proto source directory and eliminates stale duplicated
codec generated files from the active build path.
```

Implemented:

- Added vcpkg feature `codec-grpc`.
- Added CMake discovery for `gRPC::grpc_cpp_plugin`, with vcpkg tool fallback.
- Replaced the old glob-style `generate_proto` command with:
  - `generate_common_proto`
  - `generate_codec_grpc`
  - aggregate `generate_proto`
- Generated `common/proto/codec_service.grpc.pb.*`.
- Added `common/proto/codec_service.pb.cc` to `im::proto`.
- Updated `im_codec_service` to consume canonical generated files from
  `common/proto`.
- Removed duplicated `services/codec/base.pb.*` from the active target.

## Acceptance Criteria

- `cmake --build <build> --target generate_proto` regenerates active
  protobuf and codec gRPC generated files from `common/proto`.
- No active target compiles duplicated `services/codec/base.pb.*`.
- `MYCHAT_BUILD_CODEC_SERVICE=ON` builds `im_codec_service` from canonical
  generated files.
- Existing ODB-enabled baseline remains 14/14 passing.
- Existing no-ODB baseline remains 2/2 passing.
- Default `MYCHAT_BUILD_CODEC_SERVICE=OFF` behavior remains unchanged.
- No standalone Push Service behavior is implemented in this task.

Status: complete.

## Verification

Codec explicit-enable path:

```bash
cmake -S . -B /tmp/mychat-build-codec \
  -DVCPKG_MANIFEST_FEATURES=codec-grpc \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_CODEC_SERVICE=ON \
  -DMYCHAT_BUILD_GATEWAY=OFF \
  -DMYCHAT_BUILD_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-codec --target generate_proto -j2
cmake --build /tmp/mychat-build-codec --target im_codec_service -j2
```

Result:

```text
generate_proto passed.
im_codec_service passed.
```

Regression baselines:

```text
ODB-enabled: configure/build/test passed, 14/14 tests.
No-ODB: configure/build/test passed, 2/2 tests.
```

## Out Of Scope

- Implementing a running CodecService server.
- Introducing Gateway-to-service gRPC calls.
- Refactoring Friend/Group/Message business logic.
- Changing WebSocket protobuf request/response contracts.
- Changing `ProtobufCodec` wire format.
- Regenerating ODB files.
- Starting standalone Push Service implementation.

## Follow-Up

- Decide whether to delete or archive the inactive `services/codec/*.pb.*`
  duplicates in a later cleanup. They are intentionally not active in CMake
  after this phase.
- Design the standalone Push Service boundary only after preserving the current
  direct-message push and group-message fanout behavior with focused tests.

## Risks

- Regenerating `.pb.*` with the shell `protoc` 34.1 would produce files
  incompatible with the vcpkg Protobuf 5.29.3 runtime used by CMake. Always use
  the CMake-selected `Protobuf_PROTOC_EXECUTABLE`.
- Moving generated outputs out of `common/proto` would touch many include paths.
  Do that only if the plan explicitly chooses a generated output directory.
- Linking both `common/proto/base.pb.*` and `services/codec/base.pb.*` into the
  same binary can create duplicated protobuf descriptors. The active build path
  no longer does this, but the inactive duplicate files still exist.
