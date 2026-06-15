# MyChat Interview Review Plan

Date: 2026-06-09

## Goal

This phase reviews MyChat as a complete practical MVP for resume, interview,
manual validation, and later multi-machine distributed testing.

The goal is not to keep adding every production IM feature. The goal is to make
the existing optimized system understandable, testable, and defensible:

- What has been implemented.
- Which backend interface each user action exercises.
- Which technology choices matter.
- What tradeoffs were made.
- What can be discussed clearly in an interview.
- What remains as planned extensions.

## Current MVP Position

MyChat is currently a C++ distributed IM backend with a Web validation client.

Core backend capabilities:

- Gateway HTTP and WebSocket entrypoints.
- User registration, login, token-backed profile lookup, and profile update.
- Direct one-to-one message persistence, history, offline pull, WebSocket
  send/ack, and online push.
- Friend search, friend request, pending request list, respond, and friend
  list.
- Group create, search, info, join, leave, member list, group message history,
  and group online fanout.
- Push runtime with pluggable fanout policy and local/remote PushNotifier
  boundary.
- User, Message, Friend, Group, and Push services with local logic, gRPC
  adapters, standalone server processes, and Gateway local/remote facades.
- PostgreSQL plus ODB persistence for durable business state.
- Redis hiredis connection pool for token/session-related state.
- `common/proto` as the canonical protobuf/gRPC contract source.
- Web client under `clients/web` for Chinese desktop-style IM validation.

Current MVP limitations:

- Avatar is stored as text (`avatar`) and accepts URL or base64 data URL.
  Dedicated upload/object storage is a later extension.
- Group announcement, personal group nickname, group remark, pinned, mute, and
  notification setting UI are validation shapes and are not all persisted as
  backend group settings yet.
- No message recall/delete/edit/search.
- No rich media upload or file storage service.
- No blacklist/privacy rule engine.
- No full group admin system such as owner transfer, admin roles, invite/kick,
  or member mute.
- Hosted CI, deployment packaging, production TLS/secrets, and load benchmarks
  remain release/production-hardening tasks.

## Review Principles

Use the Web client as the front door for validation, but always map a UI action
back to the backend implementation:

```text
UI action -> Gateway HTTP/WS contract -> local/remote facade -> service ->
repository/runtime -> Redis/PostgreSQL/WebSocket side effect
```

Separate three categories while reviewing:

- MVP-complete: usable and demonstrable now.
- Engineering-hardening: important for production, but not blocking the
  current resume project.
- Product-extension: good future feature, but outside the current MVP.

When preparing interview answers, explain both the final design and the
migration path. The project has strong value because it evolved from local
service logic into gRPC-capable distributed boundaries without changing the
external Gateway contract.

## Guided Review Path

### 1. Top-Level Architecture

Files to read:

- `README.md`
- `docs/project/architecture/mvp_architecture.md`
- `docs/project/roadmap/service_mvp_roadmap.md`
- `docs/project/roadmap/current_progress.md`

Focus:

- Why Gateway owns external HTTP/WebSocket contracts.
- Why service boundaries are preserved even in local mode.
- How local mode and remote gRPC mode share the same Gateway-facing facade.
- Why the Web client talks only to Gateway.

Interview questions:

- Why not let the Web client call every microservice directly?
- What problem does Gateway solve in an IM system?
- How do you keep local development fast while still preserving distributed
  service boundaries?
- What is the tradeoff of putting WebSocket session ownership in Gateway?

### 2. Build, Runtime, and Configuration

Files to read:

- `scripts/dev/run_full_stack.sh`
- `scripts/dev/run_remote_services_stack.sh`
- `scripts/dev/prepare_runtime.sh`
- `config/dev.json`
- `config/dev.remote-all.json`
- `docs/project/runtime/README.md`
- `docs/history/devlog/phase18_remote_runtime_runbook.md`

Focus:

- Redis/PostgreSQL startup and schema migration policy.
- Local facade mode vs. remote all-service mode.
- Startup order and health checks.
- Why service binaries do not run migrations implicitly.

Manual validation:

- Start the full local stack.
- Confirm Gateway health.
- Start Web client and login/register two accounts.
- Record which ports are used by Gateway, WebSocket, service gRPC servers, and
  Vite.

Interview questions:

- Why are migrations kept explicit?
- How would you deploy Gateway and services onto multiple machines?
- What config changes are needed when service endpoints move?

### 3. Auth, Token, Redis, and Profile

Files to read:

- `gateway/http/user_http_controller.*`
- `gateway/http/user_client.*`
- `gateway/http/remote_user_client.*`
- `services/user/user_service.*`
- `services/user/user_repository.*`
- `services/user/user_grpc_service.*`
- `common/proto/user.proto`
- `common/redis/*`

Focus:

- Register/login flow.
- Gateway-issued token behavior.
- Redis-backed token/session state.
- Profile lookup and update chain.
- Avatar storage decision: URL/base64 in text field for MVP; upload service
  later.

Manual validation:

- Register two users.
- Login/logout/relogin.
- Open personal profile from the Web avatar.
- Edit nickname, signature, gender, and avatar.
- Refresh page and verify persistence.

Interview questions:

- Why does Gateway verify token identity instead of trusting client UID?
- How are refresh/access token states stored?
- Why use Redis for token/session state rather than PostgreSQL?
- How would you redesign avatar storage for production?

### 4. Direct Messaging

Files to read:

- `gateway/http/message_http_controller.*`
- `gateway/ws/message_ws_handler.*`
- `gateway/http/message_client.*`
- `gateway/http/remote_message_client.*`
- `services/message/message_service.*`
- `services/message/message_repository.*`
- `services/message/message_grpc_service.*`
- `common/proto/message.proto`

Focus:

- HTTP send/history/offline pull.
- WebSocket send/ack.
- Message status: sent, delivered, read.
- Offline pull marks delivered.
- Token-derived sender identity.

Manual validation:

- Send direct message while both users are online.
- Verify receiver gets realtime push without manual refresh.
- Stop or disconnect one user and send offline messages.
- Reconnect and pull/refresh history.

Interview questions:

- How do you prevent a client from spoofing sender UID?
- Why persist before push?
- What happens if push fails after persistence succeeds?
- How would you support message recall or exactly-once delivery later?

### 5. Friend Relationship Flow

Files to read:

- `gateway/http/friend_http_controller.*`
- `gateway/http/remote_friend_client.*`
- `services/friend/friend_service.*`
- `services/friend/friend_repository.*`
- `services/friend/friend_grpc_service.*`
- `common/proto/friend.proto`

Focus:

- Search-before-request user experience.
- Request lifecycle: pending, accept, reject.
- Duplicate and self-request prevention.
- Friend list as relationship state, not just contact UI.

Manual validation:

- Search user by account and fuzzy nickname.
- Send friend request.
- Accept from the second account.
- Verify both friend/contact lists.

Interview questions:

- How do you model bidirectional friendship?
- How do you avoid duplicate or reverse duplicate requests?
- What indexes would you add for account/nickname search?

### 6. Group and Group Messaging

Files to read:

- `gateway/http/group_http_controller.*`
- `gateway/http/group_message_http_controller.*`
- `gateway/http/remote_group_client.*`
- `services/group/group_service.*`
- `services/group/group_repository.*`
- `services/group/group_grpc_service.*`
- `common/proto/group.proto`

Focus:

- Group create/search/info/join/leave/member list.
- Group message persistence and history.
- Group fanout excludes the sender.
- Which group profile UI controls are backend-backed now and which are future
  settings.

Manual validation:

- Create a group.
- Search the group before joining from another account.
- Join and inspect group info/member list.
- Send group messages and verify online fanout.
- Leave group from group info.

Interview questions:

- How do you represent group membership?
- How do you prevent non-members from sending group messages?
- Why should group fanout exclude the sender?
- What changes are needed for group admin roles?

### 7. Push Runtime and Fanout

Files to read:

- `services/push/push_runtime.*`
- `services/push/fanout_policy.*`
- `services/push/push_grpc_service.*`
- `gateway/push/push_service.*`
- `gateway/push/remote_push_notifier.*`
- `gateway/push/gateway_push_delivery_service.*`
- `common/proto/push.proto`

Focus:

- PushNotifier interface.
- PushRuntime owns fanout selection and payload construction.
- Gateway owns WebSocket sessions.
- Remote Push calls back into Gateway delivery service because Gateway has the
  live WebSocket connections.
- Best-effort delivery and offline fallback.

Manual validation:

- Verify direct push with two online users.
- Verify group push with multiple online members.
- Stop remote Push server or misconfigure endpoint in a controlled test and
  confirm send does not incorrectly mark delivered.

Interview questions:

- Why is Push separated from Message?
- Why does remote Push need a Gateway callback path?
- How would you implement per-platform fanout?
- How would you scale push across multiple Gateway instances?

### 8. Proto and gRPC Boundary

Files to read:

- `common/proto/*.proto`
- root `CMakeLists.txt`
- service `CMakeLists.txt` files

Focus:

- `common/proto` is canonical.
- `generate_proto` is the aggregate generation target.
- Generated files are shared by Gateway remote clients and service gRPC
  adapters.
- Default build can stay gRPC-off for speed while explicit remote builds enable
  gRPC services.

Interview questions:

- Why use protobuf/gRPC for internal service calls?
- How do you prevent generated-file drift?
- What compatibility rules matter when evolving proto messages?

### 9. Persistence and Schema

Files to read:

- `db/migrations/001_core_schema.sql`
- `scripts/db/migrate_postgres.sh`
- `services/odb/*`
- service repository files
- `docs/modules/database/PostgreSQL_ODB_封装设计文档.md`

Focus:

- ODB maps C++ domain models to PostgreSQL.
- Migrations are explicit and tracked by checksum.
- Tests share schema setup helpers.
- Service repositories currently use direct `odb::pgsql::database`.

Interview questions:

- Why use PostgreSQL for durable IM state?
- What are the pros/cons of ODB compared with handwritten SQL?
- Why keep migration execution outside service startup?
- How would you handle schema evolution in production?

### 10. Web Validation Client

Files to read:

- `clients/web/README.md`
- `docs/project/architecture/web_validation_client_plan.md`
- `clients/web/src`

Focus:

- The Web client validates backend behavior but should feel like a normal
  Chinese IM client.
- Main flows should not expose device IDs, endpoints, internal UIDs, or raw
  backend mechanics.
- Developer/debug surfaces are available but secondary.

Manual validation:

- Two-tab/two-account direct chat.
- Friend request and contact list.
- Group search/join/group chat.
- Profile and group detail views.
- Send shortcut setting.
- Error behavior when token expires or backend is down.

Interview questions:

- Why build a Web client for a backend-focused project?
- How does the client validate WebSocket binary protobuf payloads?
- How would this flow map to Electron or Qt later?

### 11. Multi-Machine Distributed Test

This should be a later guided pass after single-machine manual validation is
boring.

Planned topology:

- Machine A: Gateway HTTP/WebSocket and Web client.
- Machine B: User, Friend, Group, Message gRPC services.
- Machine C: Push server, or colocated with Machine B for the first pass.
- Shared or dedicated PostgreSQL/Redis depending on available machines.

Key checks:

- Every `*.remote_endpoint` points to a routable address, not `127.0.0.1`, when
  crossing machine boundaries.
- `push.gateway_delivery_listen_address` is where Gateway listens for Push
  callbacks.
- `push.gateway_delivery_endpoint` is the address Push server uses to call
  Gateway back, and must be reachable from the Push server machine.
- Firewall rules allow Gateway HTTP, Gateway WebSocket, service gRPC ports,
  Push gRPC port, and Gateway Push delivery callback port.
- Timeouts and logs are checked before assuming code failure.

Interview questions:

- Which parts are stateless and easiest to scale horizontally?
- What breaks if two Gateway instances own different WebSocket sessions?
- How would you route Push to the correct Gateway instance in production?
- Where would service discovery fit?

## Suggested Resume Framing

Accurate one-line description:

```text
Built a C++ distributed IM backend with Gateway HTTP/WebSocket entrypoints,
gRPC service boundaries, Redis-backed auth/session state, PostgreSQL/ODB
persistence, realtime push fanout, and a React Web validation client.
```

Possible resume bullets:

- Designed a Gateway-centered IM architecture where external HTTP/WebSocket
  contracts remain stable while User, Message, Friend, Group, and Push services
  can run either in-process or as remote gRPC services.
- Implemented direct and group messaging with PostgreSQL persistence,
  offline-pull semantics, WebSocket send/ack, online push, and best-effort
  delivery fallback.
- Built a Push runtime with pluggable fanout policies and a remote callback
  topology that lets an independent Push service deliver through
  Gateway-owned WebSocket sessions.
- Added Redis-backed token/session state with a hiredis connection pool and
  PostgreSQL schema migration scripts for repeatable local runtime setup.
- Built a Chinese desktop-style Web validation client to exercise auth,
  profile, friend, group, direct chat, and realtime push flows end to end.

## Next Documentation Outputs

- `docs/final_sum_docs/01_项目整体架构总览.md`: 架构故事和核心链路图。
- `docs/final_sum_docs/11_单机MVP手工测试清单.md`: 用户主导的手工验证清单。
- `docs/final_sum_docs/14_多机器分布式部署验证方案.md`: 多机器部署和验证指南。
- `docs/final_sum_docs/18_高频面试问题与回答.md`: 模块级面试问题和回答。
- `docs/final_sum_docs/16_简历项目描述与亮点.md`: 简历表述和项目亮点。
