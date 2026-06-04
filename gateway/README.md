# Gateway Module Layout

Date: 2026-06-04

The Gateway is the client-facing process. It owns HTTP route registration,
WebSocket message handling, online connection state, authentication integration,
and the in-process push adapter used by the staged MVP.

## Directory Map

- `gateway_server/` - process-level composition. `GatewayServer` initializes
  HTTP, WebSocket, auth, ODB-backed services, controllers, handlers, and route
  registration.
- `http/` - REST controllers. These translate HTTP JSON requests into service
  DTO calls, enforce Bearer access-token identity, and format HTTP responses.
- `ws/` - WebSocket command handlers. These consume `UnifiedMessage` values
  parsed from protobuf envelopes and return `ProcessorResult` responses.
- `push/` - in-process online push delivery. `PushService` sends
  `CMD_PUSH_MESSAGE` payloads to sessions selected by a `FanoutPolicy`.
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
- `IM_ENABLE_MESSAGE_HTTP` - message REST routes, WebSocket send handler,
  `PushService`, and fanout policies.
- `IM_ENABLE_FRIEND_HTTP` - friend REST routes.
- `IM_ENABLE_GROUP_HTTP` - group REST routes.
- `IM_ENABLE_GROUP_MESSAGE_HTTP` - group-message REST routes and push fanout.

`gateway_server_test` / `main_example.cpp` is legacy-only and is built behind
`MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`.

## Current Invariants

- `common/proto` remains the canonical protobuf source tree.
- HTTP controllers do not trust client-supplied sender UID; the access token is
  the actor identity.
- `MessageWsHandler` owns send/ack validation and delegates online delivery to
  `PushService`.
- `PushService` is best-effort: offline or unselected sessions leave messages
  pullable through offline APIs.
- The default `AllSessionsFanoutPolicy` behavior must remain "push to every
  active session" unless a task explicitly changes that contract.
