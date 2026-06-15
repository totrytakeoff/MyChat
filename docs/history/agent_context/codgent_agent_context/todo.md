---
type: todo
status: active
updated_by: coder
---

# Todo

## Completed

- [x] Web validation client first slice - `clients/web` now contains a Vite +
  React + TypeScript desktop-style validation client with a dedicated
  login/register auth screen, Telegram-like IM workspace, configurable
  endpoints, auth/profile flows, direct-message HTTP send/history/offline pull,
  friend request/respond/list/pending panels, group create/join/leave/list/
  members/message history panels, a WebSocket connection/event debug surface,
  binary `CMD_SEND_MESSAGE` encode, `SendMessageResponse`/`PushRequest` decode,
  and a collapsible debug drawer for HTTP/WS events. `npm run build` passes
  locally.
- [x] Web validation client IM UX correction - normal user flows no longer
  expose device code, platform, endpoint settings, direct UID entry, or direct
  group ID join/leave controls. Contact flow is search by account/nickname ->
  user profile card -> friend request. Group flow is search by group name/group
  number -> group profile card -> join; leave and member list are only shown
  from joined group info. Settings now exposes Enter vs. Ctrl+Enter message
  send shortcut. Chat list and message rail display unread red badges, and
  avatars use profile avatar URLs with initial fallbacks.
- [x] User/group search remote boundary completion - User gRPC/Gateway search
  supports fuzzy account/nickname search, Group gRPC/Gateway exposes group
  search and group info, generated proto outputs were refreshed through
  `generate_proto`, and remote user/group client fake tests now cover the new
  RPC methods.
- [x] Web validation client runtime validation - Playwright two-tab validation
  passed against `scripts/dev/run_remote_services_stack.sh` and Vite
  `http://127.0.0.1:5173/`. Verified separate per-tab `deviceId`, register/
  login, account search before friend request, friend request/accept/list,
  tab-specific message/contact/group sidebars, view-aware top header, direct
  online push without manual refresh, group create/join/member list, group
  message send/history, and online group push routed by push metadata.
- [x] Profile update backend and Web profile editing - User proto/service,
  repository, gRPC adapter, Gateway local/remote client, Gateway HTTP profile
  route, and Web profile editor now support updating nickname, gender,
  signature, and avatar. Avatar currently uses the existing text field and
  supports either a URL or a browser-generated base64 data URL.
- [x] Web profile/group/contact detail refinement - Personal profile opens
  from the user avatar and shows editable centered profile content. Contact
  and group tabs use the middle list for navigation, while the right main area
  shows the selected person or group detail view instead of duplicating the
  list. Group detail uses a functional member/info/settings layout suitable
  for validating group search, join, membership, leave, and message entry.
- [x] Push routing metadata for Web validation - direct and group push now
  carry `sender_uid`, `conversation_type`, and `conversation_id` through
  `NotifyUserRequest` and `PushRequest.body.ext`. Gateway HTTP direct send,
  Gateway WS direct send, and Group Message HTTP fanout all provide this
  context; the Web client refreshes the correct conversation/group on push.
- [x] Group Service MVP - ODB schema (group + group_member), GroupRepository,
  GroupService + GroupMessageService, 17 service-level tests (14 GroupServiceTest
  cases + 3 GroupMessageServiceTest cases), Gateway HTTP controller + 23
  integration tests (GatewayGroupHttpTest), GroupMessage ODB model + HTTP
  controller (send/history) + 15 tests (GatewayGroupMessageHttpTest),
  multi-recipient group message fanout via PushService. Latest full ODB +
  Gateway + Push gRPC baseline: 23/23 suites.
- [x] Gateway remote Group client facades - Group HTTP and Group Message HTTP
  now use local/remote Gateway client facades, `GatewayServer` selects through
  `group.mode`, and remote Group Gateway smoke coverage preserves the existing
  external HTTP contracts.
- [x] Local release-candidate backend baseline - User, Message, Friend, Group,
  and Push MVPs have local service logic, gRPC service boundaries, standalone
  server processes, Gateway local/remote facades, focused tests, and local
  remote runtime documentation.

## Completed (Previous)

- [x] Message Service persistence core (Task 003) - ODB-backed `im_messages`,
  validated direct text send, chronological conversation history, offline pull,
  and delivered/read marking.
- [x] Gateway Message HTTP integration (Task 004) - authenticated send,
  conversation history, and offline pull routes; token UID is sender/actor;
  offline pull marks returned messages delivered.
- [x] Gateway WebSocket send/ack (Task 006) - CMD_SEND_MESSAGE handler,
  protobuf SendMessageRequest/SendMessageResponse, token-derived sender
  identity, protobuf type validation, cmd_id validation, protobuf error
  responses.
- [x] Gateway online message delivery (Task 007) - after successful
  persistence, pushes CMD_PUSH_MESSAGE + PushRequest to recipient's active
  sessions via ConnectionManager and WebSocketServer; marks message delivered
  after at least one successful push. Best-effort: offline recipients stay
  undelivered.
- [x] PushService with FanoutPolicy (Task 008) - extracted push logic from
  MessageWsHandler into dedicated PushService class with pluggable FanoutPolicy.
  Default AllSessionsFanoutPolicy preserves existing behavior. MessageWsHandler
  delegates to PushService::push_to_user. Independently testable PushService.
- [x] Production fanout policies - PlatformFilterFanoutPolicy (select sessions
  matching a set of allowed platforms) and NewestSessionFanoutPolicy (select
  the single most recently connected session). These now live in
  `services/push/fanout_policy.*`.
- [x] Friend Service MVP - Friend persistence model, repository, service,
  Gateway HTTP controller with focused tests (FriendServiceCoreTest 14 cases,
  GatewayFriendHttpTest 16 cases). API contract documented: send_request
  rejects EMPTY_UID, SELF_REQUEST, TARGET_NOT_FOUND, ALREADY_EXISTS;
  respond_to_request validates FORBIDDEN/NOT_PENDING/NOT_FOUND. All Friend
  HTTP routes registered before legacy catch-all.
- [x] Codec/gRPC generation chain cleanup (Task 009) - protobuf and codec gRPC
  generation are deterministic from canonical `common/proto` inputs.
  `generate_proto` covers active protobuf, codec gRPC, and push gRPC outputs
  when the gRPC plugin is available, and `im_codec_service` no longer compiles
  duplicated generated files from `services/codec`.
- [x] Inactive codec generated-file cleanup -
  Removed duplicated `services/codec/*.pb.*` outputs after verifying active
  codec builds consume canonical generated files from `common/proto`.
- [x] Push Service boundary - `services/push` now owns the
  `im::service::push::PushNotifier` boundary, service-owned fanout policies,
  and `im::push_service` target. Gateway WS and Group Message HTTP depend on
  that boundary; focused tests pin direct-message push delegation and group
  fanout recipients.
- [x] PushRuntime core extraction - `services/push/PushRuntime` now owns fanout
  selection, protobuf push payload construction, best-effort send
  orchestration, and delivered marking decisions. Gateway `PushService` is a
  narrow adapter for `ConnectionManager`, `WebSocketServer`, and Message
  Service delivered marking.
- [x] Gateway Push/WS build gating cleanup - Message HTTP remains gated only on
  Message Service; `PushService`, `MessageWsHandler`, and Group Message HTTP
  fanout now require `im::push_service`. A `MYCHAT_BUILD_SERVICES=OFF`
  Gateway-only configure/build passed.
- [x] Push gRPC remote boundary - `common/proto/push.proto` now defines
  `im.push.PushService.NotifyUser`; `generate_push_grpc` and aggregate
  `generate_proto` produce `common/proto/push.grpc.pb.*`; `im::push_grpc_service`
  adapts generated gRPC calls to the existing `PushNotifier` boundary behind
  `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
- [x] User gRPC remote boundary first slice -
  `common/proto/user.proto` now defines `im.user.UserService` with
  Register/Login/GetUserInfo; `generate_user_grpc` and aggregate
  `generate_proto` produce `common/proto/user.grpc.pb.*`; `im::user_grpc_service`
  adapts generated gRPC calls to the existing ODB-backed UserService boundary
  behind `MYCHAT_BUILD_USER_GRPC_SERVICE=ON`.
- [x] Standalone User server process slice -
  `services/user/user_server` hosts `UserGrpcService` around the ODB-backed
  `UserService` behind `MYCHAT_BUILD_USER_GRPC_SERVICE=ON`.
  `UserServerAppTest` verifies Register/Login/GetUserInfo through a generated
  gRPC stub against Docker PostgreSQL.
- [x] Gateway remote User client wiring -
  `UserHttpController` now depends on the Gateway `UserClient` facade;
  `LocalUserClient` preserves the existing in-process UserService path, and
  `RemoteUserClient` wraps the generated `im.user.UserService::Stub`.
  `GatewayServer` selects through `user.mode`, defaulting to local. Focused
  `RemoteUserClientTest` and process-level `RemoteUserGatewayServerSmokeTest`
  verify remote mapping and unchanged HTTP auth/profile contracts.
- [x] Message gRPC remote boundary first slice -
  `common/proto/message.proto` now defines `im.message.MessageService` with
  SendMessage/GetConversation/PullOffline/MarkDelivered/MarkRead;
  `generate_message_grpc` and aggregate `generate_proto` produce
  `common/proto/message.grpc.pb.*`; `im::message_grpc_service` adapts generated
  gRPC calls to the existing ODB-backed MessageService boundary behind
  `MYCHAT_BUILD_MESSAGE_GRPC_SERVICE=ON`.
- [x] Standalone Message server process slice -
  `services/message/message_server` hosts `MessageGrpcService` around the
  ODB-backed `MessageService` behind `MYCHAT_BUILD_MESSAGE_GRPC_SERVICE=ON`.
  `MessageServerAppTest` verifies send/offline-pull/delivered-marking through
  a generated gRPC stub against Docker PostgreSQL.
- [x] Gateway remote Message client wiring -
  `MessageHttpController`, `MessageWsHandler`, and `PushService` now depend on
  the Gateway `MessageClient` facade; `LocalMessageClient` preserves the
  in-process `MessageService` path, and `RemoteMessageClient` wraps the
  generated `im.message.MessageService::Stub`. `GatewayServer` selects through
  `message.mode`, defaulting to local. Focused
  `RemoteMessageClientTest` and process-level
  `RemoteMessageGatewayServerSmokeTest` verify remote mapping and unchanged
  Message HTTP contracts.
- [x] Friend gRPC boundary first slice -
  `common/proto/friend.proto` now defines `im.friend_.FriendService` with
  SendRequest/RespondToRequest/GetFriends/GetPendingRequests;
  `generate_friend_grpc` and aggregate `generate_proto` produce
  `common/proto/friend.grpc.pb.*`; `im::friend_grpc_service` adapts generated
  gRPC calls to the existing ODB-backed FriendService boundary behind
  `MYCHAT_BUILD_FRIEND_GRPC_SERVICE=ON`.
- [x] Standalone Friend server process slice -
  `services/friend/friend_server` hosts `FriendGrpcService` around the
  ODB-backed `FriendService` behind `MYCHAT_BUILD_FRIEND_GRPC_SERVICE=ON`.
  `FriendServerAppTest` verifies send/respond/list through a generated gRPC
  stub against Docker PostgreSQL.
- [x] Gateway remote Friend client wiring -
  `FriendHttpController` now depends on the Gateway `FriendClient` facade;
  `LocalFriendClient` preserves the in-process `FriendService` path, and
  `RemoteFriendClient` wraps the generated `im.friend_.FriendService::Stub`.
  `GatewayServer` selects through `friend.mode`, defaulting to local. Focused
  `RemoteFriendClientTest` and process-level
  `RemoteFriendGatewayServerSmokeTest` verify remote mapping and unchanged
  Friend HTTP contracts.
- [x] Gateway remote PushNotifier wiring - `gateway/push/RemotePushNotifier`
  wraps the generated `im.push.PushService::Stub`; `GatewayServer` selects
  local vs. remote through `push.mode`; `config/dev.json` defaults to
  `push.mode = "local"` so the existing in-process adapter remains the default.
- [x] Standalone Push server process slice - `services/push/push_server` hosts
  `PushGrpcService + PushRuntime` behind `MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON`.
  It keeps no-op adapters only when no Gateway delivery endpoint is configured.
- [x] Remote Push delivery callback channel first slice - `push.proto` now
  defines `GatewayPushDeliveryService`; Gateway remote mode exposes session
  lookup, payload send, and delivered marking on
  `push.gateway_delivery_listen_address`; `push_server` can call back through
  `push.gateway_delivery_endpoint` via remote-aware adapters.
- [x] Remote Push gRPC-link E2E smoke - `RemotePushEndToEndSmokeTest` starts a
  real Gateway delivery gRPC endpoint plus a real `PushServerApp`, calls
  `NotifyUser`, and verifies session lookup, payload send, protobuf payload
  contents, delivered marking, and offline/no-session best-effort behavior.
- [x] Remote Push Gateway entrypoint smoke - `RemotePushGatewayEntrypointsTest`
  injects a real `RemotePushNotifier` into `MessageWsHandler` and
  `GroupMessageHttpController`, then verifies direct WS and group HTTP send
  paths reach `push_server` and Gateway delivery callbacks with preserved
  direct/group fanout semantics.
- [x] Full GatewayServer process-level remote Push smoke -
  `RemotePushGatewayServerSmokeTest` starts a real `PushServerApp`, starts a
  real `GatewayServer` with `push.mode=remote`, verifies HTTP health on the
  real HTTP port, connects sender/receiver TLS WebSocket clients on the real
  WS port, sends `CMD_SEND_MESSAGE`, receives the sender ack, and verifies the
  receiver gets `CMD_PUSH_MESSAGE` through the remote Push callback path.
- [x] Push server invalid-listen startup guard - `PushServerAppTest` verifies an
  invalid `push.listen_address` returns false and does not mark the server
  running.
- [x] Remote Push local two-process config seed -
  `config/dev.remote-push.json` configures Gateway remote mode and requires
  `push_server` to use Gateway delivery callbacks instead of silently falling
  back to no-session/no-op adapters.
- [x] Push server required callback endpoint guard - `PushServerAppTest`
  verifies `push_server` startup fails when
  `require_gateway_delivery_endpoint=true` and the endpoint is blank.
- [x] PostgreSQL schema migration baseline -
  `db/migrations/001_core_schema.sql` defines the current core IM schema
  non-destructively, and `scripts/db/migrate_postgres.sh` applies ordered SQL
  migrations with version/checksum tracking in `schema_migrations`.
- [x] Service-level schema setup consolidation -
  User, Message, Friend, and Group service tests now use
  `test/support/postgres_schema.*` instead of duplicating table creation SQL.
- [x] Gateway-level schema setup consolidation -
  Focused Gateway User, Message HTTP/WS, PushService, Friend, Group, and Group
  Message tests now use `test/support/postgres_schema.*` instead of duplicating
  table creation SQL.
- [x] Remote Push smoke schema setup consolidation -
  Remote Push Gateway entrypoint and full GatewayServer smoke fixtures now use
  `test/support/postgres_schema.*`.
- [x] Low-level ODB test schema setup consolidation -
  `ODBUserPersistenceTest` now uses `test/support/postgres_schema.*` instead of
  carrying its own single-table schema SQL.
- [x] Local runtime migration policy -
  Gateway and service binaries do not run migrations implicitly; local
  development uses `scripts/dev/prepare_runtime.sh` before startup, and
  `scripts/dev/run_remote_push_stack.sh` wraps the two-process remote Push
  local stack.
- [x] Group gRPC boundary and standalone server process slice -
  `common/proto/group.proto` now defines `im.group.GroupService` for
  create/join/leave/list, member listing, group message send, and group
  message history; `generate_group_grpc` and aggregate `generate_proto`
  produce `common/proto/group.grpc.pb.*`; `im::group_grpc_service` adapts
  generated gRPC calls to the existing ODB-backed GroupService and
  GroupMessageService boundaries behind `MYCHAT_BUILD_GROUP_GRPC_SERVICE=ON`.
  `services/group/group_server` hosts that adapter, and focused
  `GroupGrpcServiceTest` plus `GroupServerAppTest` pin the service/server
  behavior against Docker PostgreSQL.

## Next

- [ ] Write the interview-oriented architecture walkthrough: one top-level
      story, module responsibilities, request-flow diagrams, and the tradeoffs
      behind Gateway, service facades, gRPC, Redis, PostgreSQL/ODB, WebSocket,
      and Push.
- [ ] Produce a manual MVP validation checklist for the user-led review phase:
      auth/profile, direct chat, offline pull, friend flow, group flow,
      realtime push, error handling, restart behavior, and Web client UX.
- [ ] Produce a multi-machine distributed runbook based on the existing
      remote-all topology: required ports, config changes, startup order,
      network reachability checks, Gateway callback address rules for Push,
      and failure scenarios to test.
- [ ] Prepare interview Q&A notes per module, including likely follow-up
      questions about consistency, offline messages, fanout policy,
      Gateway-owned WebSocket sessions, Redis token/session state, schema
      migration, and local-vs-remote service modes.
- [ ] Draft resume bullets that accurately describe the current MVP without
      overstating production readiness.
- [ ] Add optional display metadata to push payloads, especially sender
      nickname and group name, so clients can render notifications before
      refreshing history.
- [ ] Add generated browser protobuf bindings only if the explicit
      `protobufjs/minimal` adapter becomes hard to maintain.
- [ ] Decide whether the Web client should gain automated Playwright tests in
      the repo after backend/UI APIs stabilize further.

## Future Extension Pool

- [ ] Message delete, recall, and edit.
- [ ] Rich media messages: image, file, audio, video, and upload/storage
      integration.
- [ ] Blacklist and privacy rules.
- [ ] Group owner transfer, administrator management, invite, kick, and mute.
- [ ] First-class read receipt UI and delivery/read status visualization.
- [ ] Message search.
- [ ] Notification settings.
- [ ] Offline push queue visibility and retry diagnostics.
- [ ] Multi-device session management UI.
- [ ] Electron desktop shell reusing the Web app.
- [ ] Qt desktop client reusing the same Gateway HTTP/WebSocket interaction
      model.
- [ ] Redis pool sizing/load benchmark.
- [ ] Hosted CI reintroduction after feature and UI validation stabilize.
- [ ] Production TLS/certificate/secret management.
- [ ] Deployment packaging and process management.
- [ ] Stronger ODB test data isolation for future parallel CI.
- [ ] Dedicated repository-boundary review before any service migrates from
      direct `odb::pgsql::database` to `PgSqlConnection`.

## Blocked

- ODB 2.5.0 runtime build from source — not yet available in vcpkg; CMake fails at configure time if runtime is missing.
- No active blocker for codec generated-file duplication remains; the
  duplicated files under `services/codec` have been removed.
