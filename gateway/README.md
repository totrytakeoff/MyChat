# Gateway Module Layout

Date: 2026-06-04

The Gateway is the client-facing process. It owns HTTP route registration,
WebSocket message handling, online connection state, authentication integration,
and PushNotifier composition for local in-process or explicit remote push.

## Directory Map

- `gateway_server/` - process-level composition. `GatewayServer` initializes
  HTTP, WebSocket, auth, ODB-backed services, controllers, handlers, and route
  registration.
- `http/` - REST controllers. These translate HTTP JSON requests into service
  DTO calls, enforce Bearer access-token identity, and format HTTP responses.
- `ws/` - WebSocket command handlers. These consume `UnifiedMessage` values
  parsed from protobuf envelopes and return `ProcessorResult` responses.
- `push/` - push notifier implementations. `PushService` is the local
  in-process adapter that sends `CMD_PUSH_MESSAGE` payloads to sessions
  selected by service-owned fanout policies from `services/push`;
  `RemotePushNotifier` is the explicit gRPC client path.
- `auth/` - multi-platform access/refresh token manager and auth helpers.
- `connection_manager/` - online device/session registry used by push delivery.
- `message_processor/` - protocol normalization and command dispatch helpers.
- `router/` - legacy/configurable route parser used by older Gateway paths.

Historical demo/test folders under `auth/`, `connection_manager/`,
`message_processor/`, and `router/` are intentionally left in place. They are
not part of the current focused Gateway cleanup.

## Build Gating

`im_gateway_core` always builds the core Gateway components. Service-backed
features are added only when their service targets exist:

- `IM_ENABLE_USER_HTTP` - user REST routes.
- `IM_ENABLE_MESSAGE_HTTP` - message REST routes.
- `IM_ENABLE_PUSH_SERVICE` - local Gateway `PushService` runtime adapter.
- `IM_ENABLE_REMOTE_PUSH_NOTIFIER` - remote Gateway `PushNotifier` gRPC client.
- `IM_ENABLE_MESSAGE_WS` - WebSocket send handler.
- `IM_ENABLE_FRIEND_HTTP` - friend REST routes.
- `IM_ENABLE_GROUP_HTTP` - group REST routes.
- `IM_ENABLE_GROUP_MESSAGE_HTTP` - group-message REST routes and push fanout.

`gateway_server_test` / `main_example.cpp` is legacy-only and is built behind
`MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`.

## Current Invariants

- `common/proto` remains the canonical protobuf source tree.
- HTTP controllers do not trust client-supplied sender UID; the access token is
  the actor identity.
- `MessageWsHandler` owns send/ack validation and delegates online delivery via
  `im::service::push::PushNotifier`.
- `GatewayServer` defaults to local push via `push.mode = "local"` and only
  selects `RemotePushNotifier` when an explicit gRPC build provides it and
  `push.mode = "remote"` is configured.
- In `push.mode = "remote"`, Gateway still owns WebSocket sessions. It exposes
  `GatewayPushDeliveryService` on `push.gateway_delivery_listen_address` so
  `push_server` can call back for session lookup, payload send, and delivered
  marking through `push.gateway_delivery_endpoint`.
- `PushService` is best-effort: offline or unselected sessions leave messages
  pullable through offline APIs.
- The default `AllSessionsFanoutPolicy` in `services/push` must remain "push to
  every active session" unless a task explicitly changes that contract.

## Remote Push Endpoint Topology

Remote Push mode is an explicit two-process topology:

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

- `push.listen_address` belongs to `push_server`. It is where the standalone
  Push service accepts `NotifyUser` RPCs.
- `push.remote_endpoint` belongs to Gateway configuration. It is the client
  target used by `RemotePushNotifier` to call `push_server`.
- `push.gateway_delivery_listen_address` belongs to Gateway configuration. It
  is where Gateway exposes `GatewayPushDeliveryService` so Push can call back
  into Gateway-owned session lookup, payload send, and delivered marking.
- `push.gateway_delivery_endpoint` belongs to `push_server` configuration. It
  is the client target used by Push server remote adapters to call Gateway.

The local development pairing is normally:

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
