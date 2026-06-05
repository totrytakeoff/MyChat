# Phase 15: Push Service Boundary

Date: 2026-06-04

Updated: 2026-06-05

## Purpose

Prepare the standalone Push Service migration without changing current delivery
behavior. This phase introduces a narrow service boundary and characterization
tests so later work can move push runtime code out of Gateway incrementally.

## Implemented State

New service module:

- `services/push/push_notifier.hpp`
- `services/push/fanout_policy.hpp`
- `services/push/fanout_policy.cpp`
- `services/push/push_runtime.hpp`
- `services/push/push_runtime.cpp`
- `services/push/push_grpc_service.hpp`
- `services/push/push_grpc_service.cpp`
- `services/push/push_server_adapters.hpp`
- `services/push/push_server_adapters.cpp`
- `services/push/push_server_app.hpp`
- `services/push/push_server_app.cpp`
- `services/push/push_server_main.cpp`
- `services/push/CMakeLists.txt`
- `im::push_service` CMake target
- `im::push_grpc_service` CMake target, gated by
  `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`
- `push_server` executable target, gated by
  `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`
- `MYCHAT_BUILD_PUSH_SERVICE` root CMake option, ON by default
- `MYCHAT_BUILD_PUSH_GRPC_SERVICE` root CMake option, OFF by default

Boundary contract:

```cpp
namespace im::service::push {

class PushNotifier {
public:
    virtual ~PushNotifier() = default;
    virtual void notify_user(const std::string& receiver_uid,
                             uint64_t msg_id,
                             const std::string& content) = 0;
};

}
```

Remote contract:

- `common/proto/push.proto` now defines service-to-service RPC
  `im.push.PushService.NotifyUser`.
- `NotifyUserRequest` carries `receiver_uid`, persisted `msg_id`, and
  `content`, matching the current `PushNotifier::notify_user` boundary.
- `NotifyUserResponse` embeds `im.base.BaseResponse`.
- `common/proto/push.pb.*` and `common/proto/push.grpc.pb.*` were regenerated
  through CMake `generate_proto` using the CMake-selected vcpkg `protoc`.
- Root CMake generalized gRPC generation from codec-only to a list containing
  `codec_service` and `push`. The legacy-compatible `generate_codec_grpc`
  target name is preserved, and `generate_push_grpc` is now available.
- `services/push/PushGrpcService` adapts the generated gRPC service to an
  injected `PushNotifier*`. It validates required fields and reports errors
  through `BaseResponse`; actual best-effort delivery remains owned by
  `PushNotifier`/`PushRuntime`.

Current runtime adapter:

- `gateway/push/PushService` implements `im::service::push::PushNotifier`.
- `services/push/PushRuntime` owns the service-side push workflow: session
  fanout selection, protobuf push payload construction, best-effort send
  orchestration, and delivered marking after at least one successful send.
- `gateway/push/PushService` is now a thin adapter. It implements
  `PushSessionProvider`, `PushPayloadSender`, and `PushDeliveryMarker` by
  delegating to Gateway `ConnectionManager`, `WebSocketServer`, and
  Message Service respectively.
- `MessageWsHandler` depends on `PushNotifier*`, not concrete `PushService*`.
- `GroupMessageHttpController` depends on `PushNotifier*`, not concrete
  `PushService*`.
- `gateway/push/RemotePushNotifier` implements the same `PushNotifier`
  boundary with a generated `im.push.PushService::Stub`.
- `GatewayServer` owns a single `PushNotifier*` injection point. By default it
  constructs the current in-process `PushService`; when `push.mode` is
  configured as `remote` and `IM_ENABLE_REMOTE_PUSH_NOTIFIER` is compiled in,
  it constructs `RemotePushNotifier` instead.
- `config/dev.json` defaults to `push.mode = "local"`, with
  `push.listen_address`, `push.remote_endpoint`, and `push.timeout_ms` present
  for the explicit remote path. Default builds still do not require gRPC.
- `push_server` is now a standalone process target that hosts
  `PushGrpcService + PushRuntime` and reads `push.listen_address`.
- The standalone server uses no-session/no-op adapters when no Gateway
  delivery endpoint is configured. When `push.gateway_delivery_endpoint` is set,
  it uses remote Gateway adapters for session lookup, session payload send, and
  delivered marking.
- Gateway remote mode starts an internal `GatewayPushDeliveryService` endpoint
  on `push.gateway_delivery_listen_address`, keeping WebSocket session
  ownership inside Gateway while allowing `push_server` to call back for
  delivery primitives.

Remote Push endpoint topology:

```text
GatewayServer
  - client-facing HTTP/WebSocket process
  - owns WebSocket sessions and delivered marking
  - listens for PushServer callbacks on push.gateway_delivery_listen_address

PushServer
  - internal Push gRPC process
  - listens for Gateway notify calls on push.listen_address
  - calls Gateway delivery callbacks through push.gateway_delivery_endpoint

GatewayServer -> PushServer:
  RemotePushNotifier -> im.push.PushService.NotifyUser
  target: push.remote_endpoint

PushServer -> GatewayServer:
  Remote Gateway delivery adapters -> im.push.GatewayPushDeliveryService
  target: push.gateway_delivery_endpoint
```

Endpoint ownership:

- `push.listen_address` belongs to `push_server`; it is the service listen
  address for `im.push.PushService.NotifyUser`.
- `push.remote_endpoint` belongs to Gateway; it is the client target used by
  `RemotePushNotifier` to call `push_server`.
- `push.gateway_delivery_listen_address` belongs to Gateway; it is where
  Gateway exposes the callback service for session lookup, payload send, and
  delivered marking.
- `push.gateway_delivery_endpoint` belongs to `push_server`; it is the client
  target used by Push server remote adapters to call Gateway.

Local remote-mode pairing:

```json
{
  "push": {
    "mode": "remote",
    "listen_address": "0.0.0.0:9101",
    "remote_endpoint": "127.0.0.1:9101",
    "gateway_delivery_listen_address": "127.0.0.1:9102",
    "gateway_delivery_endpoint": "127.0.0.1:9102"
  }
}
```

Operational rules:

- Default builds and `config/dev.json` keep `push.mode = "local"`, so no gRPC
  Push service is required by default.
- `push.mode = "remote"` requires an explicit gRPC build with
  `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
- In remote mode, Gateway startup fails if `push.remote_endpoint` or
  `push.gateway_delivery_listen_address` is blank.
- A configured but unavailable `push.remote_endpoint` does not prevent Gateway
  startup. Send remains best-effort after persistence: sender ack stays
  successful, and the recipient message remains `SENT` and offline-pullable
  unless Push later completes delivery.

Gateway build gating:

- `IM_ENABLE_MESSAGE_HTTP` is only for Message HTTP routes.
- `IM_ENABLE_PUSH_SERVICE` gates the current Gateway runtime adapter
  (`gateway/push/PushService`) and requires `im::message_service` plus
  `im::push_service`.
- `IM_ENABLE_MESSAGE_WS` gates `MessageWsHandler` registration and requires
  the same Message Service plus Push boundary targets.
- Group Message HTTP fanout requires `im::group_service`,
  `im::message_service`, and `im::push_service`.
- A Gateway-only `MYCHAT_BUILD_SERVICES=OFF` configure/build was verified so
  disabled service modules do not leave Gateway in a half-enabled Push state.

Behavior preserved:

- WebSocket direct-message send/ack behavior is unchanged.
- Successful direct-message persistence still triggers best-effort recipient
  push after ack construction.
- Push failures remain non-blocking for sender ack.
- Group message send still fans out to all other group members and does not
  push to the sender.
- `AllSessionsFanoutPolicy` remains the default production fanout policy and
  now lives in `services/push`.

## Tests Added

- `GatewayMessageWsTest.SuccessfulSendNotifiesReceiverThroughBoundary`
  verifies successful WebSocket direct-message send notifies exactly the
  receiver through `PushNotifier`, with the persisted message id and content.
- `GatewayGroupMessageHttpTest.SendMessageFansOutToOtherMembersOnly` verifies
  group-message send notifies other members only, excludes the sender, and
  passes the persisted group message id and content.
- `PushRuntimeTest` verifies service-side runtime behavior without Redis or
  PostgreSQL:
  - no sessions do not send or mark delivered;
  - selected sessions receive a non-empty encoded payload;
  - delivered marking happens only after a successful send;
  - null runtime dependencies return silently.
- `PushGrpcServiceTest` verifies the gRPC adapter:
  - valid `NotifyUser` requests delegate to `PushNotifier`;
  - missing required fields do not call the notifier and return `PARAM_ERROR`;
  - missing notifier returns `SERVER_ERROR`;
  - notifier exceptions are converted to `BaseResponse` errors.
- `RemotePushNotifierTest` verifies the Gateway remote client:
  - `notify_user` builds the expected `NotifyUserRequest`;
  - RPC transport failure is logged and remains non-throwing;
  - non-success `BaseResponse` is logged and remains non-throwing;
  - a missing injected RPC client is handled as best-effort no-op.
- `PushServerAppTest` verifies the standalone server app can bind an ephemeral
  local port and serve `NotifyUser` successfully through a generated gRPC stub.
- `GatewayPushDeliveryServiceTest` verifies Gateway's internal delivery gRPC
  adapter delegates to session lookup, payload send, and delivered marking
  dependencies while converting validation/exception failures to `BaseResponse`
  errors.
- `PushServerRemoteAdaptersTest` verifies the standalone server's remote
  Gateway adapters call `GatewayPushDeliveryService` and preserve best-effort
  empty/false behavior on transport or `BaseResponse` failures.

Existing tests continue to cover:

- no push infrastructure leaves direct messages undelivered;
- ConnectionManager with no live sessions leaves messages undelivered;
- `PushServiceTest` fanout policies and null-dependency behavior.

## Verification

Dependencies:

```bash
docker start mychat-redis mychat-postgres
```

ODB-enabled configure/build/test:

```bash
cmake -S . -B /tmp/mychat-build-review \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review -j2
ctest --test-dir /tmp/mychat-build-review --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 15
```

Focused push-related tests:

```bash
ctest --test-dir /tmp/mychat-build-review \
  -R "GatewayMessageWsTest|GatewayGroupMessageHttpTest|PushServiceTest" \
  --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 3
```

No-ODB configure/build/test:

```bash
cmake -S . -B /tmp/mychat-build-review-noodb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-review-noodb -j2
ctest --test-dir /tmp/mychat-build-review-noodb --output-on-failure
```

Result:

```text
100% tests passed, 0 tests failed out of 3
```

Gateway-only services-off smoke:

```bash
cmake -S . -B /tmp/mychat-build-gateway-noservices \
  -DMYCHAT_BUILD_TESTS=OFF \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-gateway-noservices -j2
```

Result:

```text
Configure passed; PushService, Message WS handler, and Group Message HTTP were
disabled because service targets were unavailable. Build passed.
```

Push gRPC explicit-enable smoke:

```bash
cmake -S . -B /tmp/mychat-build-pushgrpc-serial \
  -DVCPKG_MANIFEST_FEATURES=codec-grpc \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_CODEC_SERVICE=ON \
  -DMYCHAT_BUILD_GATEWAY=OFF \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-pushgrpc-serial --target generate_proto -j2
cmake --build /tmp/mychat-build-pushgrpc-serial --target im_push_grpc_service -j2
cmake --build /tmp/mychat-build-pushgrpc-serial --target test_push_runtime -j2
cmake --build /tmp/mychat-build-pushgrpc-serial --target test_push_grpc_service -j2
ctest --test-dir /tmp/mychat-build-pushgrpc-serial -R "Push" --output-on-failure
cmake --build /tmp/mychat-build-pushgrpc-serial --target im_codec_service -j2
```

Result:

```text
generate_proto passed.
im_push_grpc_service passed.
PushRuntimeTest and PushGrpcServiceTest passed, 2/2.
im_codec_service passed with the generalized gRPC generation path.
```

Push gRPC gating smoke:

```bash
cmake -S . -B /tmp/mychat-build-pushgrpc-gating \
  -DVCPKG_MANIFEST_FEATURES=codec-grpc \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PUSH_SERVICE=OFF \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_CODEC_SERVICE=OFF \
  -DMYCHAT_BUILD_GATEWAY=OFF \
  -DMYCHAT_BUILD_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-pushgrpc-gating --target im_push_grpc_service -j2
```

Result:

```text
Configure passed and entered services/push even with MYCHAT_BUILD_PUSH_SERVICE=OFF.
im_push_grpc_service passed.
```

No-ODB default regression:

```bash
cmake -S . -B /tmp/mychat-build-default-regression \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-default-regression -j2
docker start mychat-redis
ctest --test-dir /tmp/mychat-build-default-regression --output-on-failure
```

Result:

```text
Build passed.
100% tests passed, 0 tests failed out of 3.
```

Gateway remote PushNotifier explicit-enable smoke:

```bash
cmake -S . -B /tmp/mychat-build-remote-push \
  -DVCPKG_MANIFEST_FEATURES=codec-grpc \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build-remote-push --target test_remote_push_notifier -j2
cmake --build /tmp/mychat-build-remote-push --target gateway_server -j2
ctest --test-dir /tmp/mychat-build-remote-push -R "RemotePushNotifier|Push" --output-on-failure
```

Result:

```text
Remote PushNotifier enabled (im::push_grpc_service available).
gateway_server built.
PushRuntimeTest, PushGrpcServiceTest, and RemotePushNotifierTest passed, 3/3.
```

Full ODB + Gateway + Push gRPC regression:

```bash
cmake -S . -B build/remote-push-odb \
  -DVCPKG_MANIFEST_FEATURES="pgsql-odb;codec-grpc" \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PUSH_GRPC_SERVICE=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
TMPDIR=/home/myself/workspace/MyChat/build/tmp \
  cmake --build build/remote-push-odb -j2
docker start mychat-redis mychat-postgres
ctest --test-dir build/remote-push-odb --output-on-failure
```

Result:

```text
Configure enabled Message WS, Group Message HTTP, local PushService, and
Remote PushNotifier. Build passed. Full CTest passed, 22/22.
```

Note: a first attempt under `/tmp/mychat-build-remote-push-odb` failed because
the `/tmp` tmpfs quota was exhausted while vcpkg/GCC wrote temporary files. The
successful verification used the ignored repository `build/` directory and set
`TMPDIR` to `build/tmp`.

Standalone Push server focused re-verification:

```bash
git diff --check
cmake -S . -B build/default-regression \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/default-regression -j2
ctest --test-dir build/default-regression --output-on-failure
TMPDIR=/home/myself/workspace/MyChat/build/tmp \
  cmake --build build/remote-push-odb --target push_server test_push_server_app -j2
ctest --test-dir build/remote-push-odb -R Push --output-on-failure
```

Result:

```text
git diff --check passed.
Default no-ODB/no-gRPC build passed.
Default no-ODB/no-gRPC CTest passed, 3/3.
push_server, test_push_server_app, test_gateway_push_delivery_service,
test_push_server_remote_adapters, test_remote_push_e2e_smoke, and
test_remote_push_gateway_entrypoints built.
PushRuntimeTest, PushGrpcServiceTest, PushServerAppTest,
PushServerRemoteAdaptersTest, GatewayPushDeliveryServiceTest,
RemotePushEndToEndSmokeTest, RemotePushGatewayEntrypointsTest,
RemotePushNotifierTest, and PushServiceTest passed, 9/9.
```

Remote delivery plumbing first-slice verification:

```bash
cmake --build build/remote-push-odb --target generate_proto -j2
TMPDIR=/home/myself/workspace/MyChat/build/tmp \
  cmake --build build/remote-push-odb \
    --target gateway_server push_server \
             test_gateway_push_delivery_service test_push_server_remote_adapters -j2
ctest --test-dir build/remote-push-odb -R "Push" --output-on-failure
cmake -S . -B build/default-regression \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/default-regression -j2
ctest --test-dir build/default-regression --output-on-failure
```

Result:

```text
generate_proto passed.
gateway_server, push_server, test_gateway_push_delivery_service,
test_push_server_remote_adapters, test_remote_push_e2e_smoke, and
test_remote_push_gateway_entrypoints built.
PushRuntimeTest, PushGrpcServiceTest, PushServerAppTest,
PushServerRemoteAdaptersTest, GatewayPushDeliveryServiceTest,
RemotePushEndToEndSmokeTest, RemotePushNotifierTest,
RemotePushGatewayEntrypointsTest, and PushServiceTest passed, 9/9.
Default no-ODB/no-gRPC configure/build/test passed, 3/3.
```

Remote Push gRPC-link smoke verification:

```bash
TMPDIR=/home/myself/workspace/MyChat/build/tmp \
  cmake --build build/remote-push-odb \
    --target test_push_server_app test_remote_push_e2e_smoke -j2
ctest --test-dir build/remote-push-odb \
  -R "PushServerApp|RemotePushEndToEndSmoke" --output-on-failure
ctest --test-dir build/remote-push-odb -R "Push" --output-on-failure
```

Result:

```text
test_push_server_app and test_remote_push_e2e_smoke built.
PushServerAppTest and RemotePushEndToEndSmokeTest passed, 2/2.
PushRuntimeTest, PushGrpcServiceTest, PushServerAppTest,
PushServerRemoteAdaptersTest, GatewayPushDeliveryServiceTest,
RemotePushEndToEndSmokeTest, RemotePushGatewayEntrypointsTest,
RemotePushNotifierTest, and PushServiceTest passed, 9/9.
```

Gateway entrypoint remote Push smoke verification:

```bash
TMPDIR=/home/myself/workspace/MyChat/build/tmp \
  cmake --build build/remote-push-odb \
    --target test_remote_push_gateway_entrypoints -j2
ctest --test-dir build/remote-push-odb \
  -R "RemotePushGatewayEntrypoints" --output-on-failure
ctest --test-dir build/remote-push-odb -R "Push" --output-on-failure
ctest --test-dir build/remote-push-odb --output-on-failure
```

Result:

```text
test_remote_push_gateway_entrypoints built.
RemotePushGatewayEntrypointsTest passed, 1/1.
Push focused tests passed, 9/9.
Full ODB + Gateway + Push gRPC CTest passed, 22/22.
```

Full GatewayServer process-level remote Push smoke verification:

```bash
TMPDIR=/home/myself/workspace/MyChat/build/tmp \
  cmake --build build/remote-push-odb \
    --target test_remote_push_gateway_server_smoke -j2
ctest --test-dir build/remote-push-odb \
  -R "RemotePushGatewayServerSmokeTest" --output-on-failure
ctest --test-dir build/remote-push-odb -R "Push" --output-on-failure
ctest --test-dir build/remote-push-odb --output-on-failure
ctest --test-dir build/default-regression --output-on-failure
```

Result:

```text
test_remote_push_gateway_server_smoke built.
RemotePushGatewayServerSmokeTest passed, 1/1.
Push focused tests passed, 10/10.
Full ODB + Gateway + Push gRPC CTest passed, 23/23.
No-ODB default CTest passed, 3/3.
```

Remote Push Gateway startup/config hardening:

- `GatewayServer` remote mode now requires explicit non-blank
  `push.remote_endpoint`.
- `GatewayServer` remote mode now requires explicit non-blank
  `push.gateway_delivery_listen_address`.
- `start_gateway_push_delivery_server()` now fails startup for blank listen
  addresses instead of silently skipping the internal Gateway delivery gRPC
  endpoint.
- `RemotePushGatewayServerSmokeTest` now includes startup-failure coverage for
  both missing/empty required Gateway remote-mode addresses while preserving
  the real WS remote Push smoke.
- `RemotePushGatewayServerSmokeTest` also covers a configured but unavailable
  `push_server`: Gateway still starts, WS send returns the existing
  best-effort success ack, and the recipient message remains `SENT` and
  pullable through the offline path instead of being incorrectly marked
  delivered.
- `RemotePushGatewayServerSmokeTest` now covers real Gateway HTTP group send
  through remote Push: the test starts a real `GatewayServer` and
  `PushServerApp`, creates test users/groups through service helpers, posts to
  `/api/v1/groups/messages/send` on the real Gateway HTTP port, and verifies
  online group members receive `CMD_PUSH_MESSAGE` through the
  `RemotePushNotifier -> PushServerApp -> GatewayPushDeliveryService ->
  WebSocket` path.

Verification:

```bash
cmake --build build/remote-push-odb \
  --target test_remote_push_gateway_server_smoke -j2
ctest --test-dir build/remote-push-odb \
  -R RemotePushGatewayServerSmokeTest --output-on-failure
ctest --test-dir build/remote-push-odb -R Push --output-on-failure
ctest --test-dir build/remote-push-odb --output-on-failure
ctest --test-dir build/default-regression --output-on-failure
git diff --check
```

Result:

```text
test_remote_push_gateway_server_smoke built.
RemotePushGatewayServerSmokeTest passed, 1/1.
Push focused tests passed, 10/10.
Full ODB + Gateway + Push gRPC CTest passed, 23/23.
No-ODB default CTest passed, 3/3.
git diff --check passed.
```

## Remaining Work

- Promote the remote Push smoke coverage into CI once the environment contract
  for Redis/PostgreSQL/gRPC builds is settled.
- Consider a schema migration framework before adding broader persistence
  evolution; current focused tests still create missing tables defensively.
- Keep Gateway adapter responsibilities narrow: session lookup, payload send,
  and delivered marking.
- Keep direct-message and group-message characterization tests passing during
  migration.
