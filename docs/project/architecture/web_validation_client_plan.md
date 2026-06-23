# MyChat Web Validation Client Plan

Date: 2026-06-07

## Goal

The next project phase should build a Web validation client for the existing
MyChat backend. This client is not a marketing page and not a final consumer
product. It is a practical desktop-style IM UI used to exercise the backend
through real HTTP and WebSocket flows.

The first implementation target is:

```text
clients/web
```

Future clients may reuse the same product flow and API adapter ideas:

- Qt desktop client.
- Electron desktop client.
- Mobile or embedded clients.

The Web client should validate the backend before those clients are attempted.

## Product Position

The UI style should reference desktop QQ, Telegram Desktop, and similar IM
clients:

- a dedicated authentication screen for register/login before entering the IM
  workspace;
- left navigation for accounts, contacts, groups, and debug views;
- conversation list next to the primary navigation;
- main chat panel with message history and composer;
- right-side optional detail/debug drawer;
- restrained desktop tool styling rather than a landing page;
- dense, readable operational layout that helps testing multiple users quickly.

The main workspace must feel like an IM client first, not a backend debug
dashboard. Account/password/register/login fields must not be permanently
visible after login. Backend validation tools remain available through
settings/debug panels and drawers, but they should not displace the core chat
experience.

This project should prioritize backend verification over visual novelty. The UI
needs to make failures obvious: authentication errors, WebSocket disconnects,
HTTP response payloads, push events, and backend endpoint settings should be
visible without opening browser DevTools for every test.

## Technical Direction

Recommended stack for the first Web client:

- React + TypeScript + Vite.
- Browser `fetch` for Gateway HTTP APIs.
- Browser `WebSocket` for Gateway realtime entry.
- `protobufjs/minimal` or generated JavaScript/TypeScript protobuf bindings for
  WebSocket payloads. The first implemented slice uses an explicit minimal wire
  adapter for the active `IMHeader`, message, push, and base response fields.
- Local storage for development session persistence only.
- No heavy UI framework required for the first pass; use scoped CSS modules or
  plain CSS with a small component structure.

The implementation should keep an API adapter layer between UI components and
Gateway contracts. That adapter should be portable enough for future Qt or
Electron clients to copy the interaction model, even though they will not reuse
React components directly.

## Backend Entry Points

Default development backend:

```text
HTTP:      http://127.0.0.1:8102
WebSocket: wss://127.0.0.1:8101
```

The Vite development server should proxy `/api/*` to Gateway so the browser can
validate HTTP flows without requiring Gateway CORS headers during local
development. When served outside Vite, the client may use an explicit HTTP base
URL, but Gateway must then allow that browser origin.

The Vite development server should also proxy `/ws` to Gateway's TLS WebSocket
port with local certificate verification disabled. The browser default should
therefore be the current same-origin Vite host plus `/ws`, for example
`ws://127.0.0.1:5173/ws` or `ws://127.0.0.1:5174/ws` if Vite selects another
port. Direct Gateway access remains `wss://127.0.0.1:8101` after trusting the
development certificate.

The Web client must make these endpoints configurable from the post-login
Developer panel or environment defaults. The normal auth screen, chat
workspace, and account settings page should not expose endpoint fields. It
should work against both:

- `config/dev.json` default local-facade Gateway mode;
- `config/dev.remote-all.json` all-service remote mode.

The client should not know whether Gateway is calling local services or remote
gRPC services. That is a backend deployment detail.

## Supported Gateway HTTP Contracts

All authenticated routes use:

```text
Authorization: Bearer <access_token>
```

### Auth

`POST /api/v1/auth/register`

Request:

```json
{
  "account": "alice",
  "password": "pass",
  "nickname": "Alice",
  "device_id": "web-dev-1",
  "platform": "web"
}
```

Response:

```json
{
  "profile": {
    "uid": "...",
    "account": "alice",
    "nickname": "Alice"
  },
  "access_token": "...",
  "refresh_token": "..."
}
```

`POST /api/v1/auth/login`

Request:

```json
{
  "account": "alice",
  "password": "pass",
  "device_id": "web-dev-1",
  "platform": "web"
}
```

Response shape matches register.

`GET /api/v1/auth/info`

Returns the current token user's profile.

`POST /api/v1/auth/profile`

Updates the current token user's editable profile fields. The authenticated
token UID is authoritative; client-supplied UID or account values must not be
trusted for this workflow.

Request:

```json
{
  "nickname": "Alice",
  "avatar": "https://example.test/avatar.png",
  "gender": 0,
  "signature": "hello"
}
```

Response:

```json
{
  "profile": {
    "account": "alice",
    "nickname": "Alice",
    "avatar": "https://example.test/avatar.png",
    "gender": 0,
    "signature": "hello"
  }
}
```

### Direct Messages

`POST /api/v1/messages/send`

Request:

```json
{
  "receiver_uid": "...",
  "content": "hello"
}
```

Response:

```json
{
  "message": {
    "msg_id": 1,
    "sender_uid": "...",
    "receiver_uid": "...",
    "content": "hello",
    "msg_type": 0,
    "status": 0,
    "create_time": 0,
    "delivered_time": 0,
    "read_time": 0
  }
}
```

`GET /api/v1/messages/history?peer_uid=<uid>&before_time=<ms>&limit=50`

Returns:

```json
{
  "messages": [],
  "count": 0
}
```

`GET /api/v1/messages/offline?before_time=<ms>&limit=50`

Returns undelivered direct messages for the current user and marks returned
messages delivered.

### Friends

`POST /api/v1/friends/request`

Request:

```json
{
  "target_uid": "..."
}
```

`POST /api/v1/friends/respond`

Request:

```json
{
  "friend_id": 1,
  "accept": true
}
```

`GET /api/v1/friends`

Returns:

```json
{
  "friends": [
    {
      "friend_id": 1,
      "friend_uid": "...",
      "nickname": "...",
      "status": 1,
      "created_at": 0
    }
  ]
}
```

`GET /api/v1/friends/pending`

Returns:

```json
{
  "pending_requests": []
}
```

### Groups

`POST /api/v1/groups`

Request:

```json
{
  "name": "test group"
}
```

`POST /api/v1/groups/join`

Request:

```json
{
  "group_id": 1
}
```

`POST /api/v1/groups/leave`

Request:

```json
{
  "group_id": 1
}
```

`GET /api/v1/groups`

Returns:

```json
{
  "groups": [
    {
      "group_id": 1,
      "name": "...",
      "creator_uid": "...",
      "created_at": 0,
      "member_count": 1
    }
  ]
}
```

`GET /api/v1/groups/members?group_id=<id>`

Returns:

```json
{
  "members": [
    {
      "id": 1,
      "user_uid": "...",
      "nickname": "...",
      "role": 2,
      "joined_at": 0
    }
  ]
}
```

### Group Messages

`POST /api/v1/groups/messages/send`

Request:

```json
{
  "group_id": 1,
  "content": "hello group"
}
```

Response:

```json
{
  "message": "...",
  "msg_id": 1
}
```

`GET /api/v1/groups/messages/history?group_id=<id>&before=<ms>&limit=50`

Returns:

```json
{
  "messages": [
    {
      "msg_id": 1,
      "sender_uid": "...",
      "sender_nickname": "...",
      "content": "...",
      "created_at": 0
    }
  ]
}
```

## WebSocket Contract

The Gateway WebSocket path currently handles protobuf-framed IM messages.

Required first slice:

- connect with the current user's token/device context in the way supported by
  Gateway;
- send `CMD_SEND_MESSAGE` using `im.message.SendMessageRequest`;
- decode sender ACK as `im.message.SendMessageResponse`;
- decode receiver push as `CMD_PUSH_MESSAGE` with `im.push.PushRequest`.

The first Web implementation should include a small protocol adapter module:

```text
src/api/wsClient.ts
src/api/protobufEnvelope.ts
```

That module owns protobuf encode/decode details so the rest of the UI only sees
typed events:

- `ws:connected`
- `ws:disconnected`
- `message:ack`
- `message:push`
- `message:error`
- `raw:frame`

The current first slice implements the backend envelope directly in the browser:

```text
varint(header_size) + varint(type_name_size) + type_name + header + payload + crc32
```

Runtime validation against a live Gateway has passed for the first slice. Keep
this adapter isolated so a future generated-browser-protobuf migration can be
done without rewriting UI components.

## UI Layout

The first viewport before login is the authentication screen. It should provide
only the normal account flows:

- segmented login/register mode switch;
- account/password inputs;
- nickname only for register;
- device/platform generated internally for backend requests;
- no endpoint fields, protocol switches, or developer controls.

Endpoint overrides and HTTP/WebSocket send-channel switches belong only in the
post-login Developer panel or debug drawer. They must not appear on the auth
screen, chat workspace, or normal account settings page.

The first viewport after login should be the actual IM workspace. It must not
show the login/register/account form as a normal sidebar panel.

Recommended desktop layout:

```text
+----------+-------------------+--------------------------------+------------+
| rail     | conversations     | active chat                    | debug      |
|          | contacts/groups   | message history + composer     | drawer     |
+----------+-------------------+--------------------------------+------------+
```

### Left Rail

Purpose: quick mode switching.

Items:

- current user avatar/status;
- Chats;
- Friends;
- Groups;
- Debug;
- Settings.

### Conversation Pane

Purpose: select one direct peer or group.

Required states:

- empty state with action buttons;
- selected direct conversation;
- selected group conversation;
- unread or push marker if easy to derive;
- loading and error state.

### Chat Pane

Purpose: validate message send/history/push.

Required controls:

- history load button or automatic load on selection;
- message list with sender direction;
- composer input;
- send button;
- online/WebSocket status indicator.

The first version should support text messages only. Other message types remain
future backend/client work.

### Friends Pane

Purpose: validate Friend Service.

Required controls:

- input target UID;
- send friend request;
- pending request list;
- accept/reject buttons;
- friend list refresh.

### Groups Pane

Purpose: validate Group Service and group message flow.

Required controls:

- create group;
- join group by ID;
- leave selected group;
- refresh my groups;
- refresh members;
- open group chat;
- send group message.

### Debug Drawer

Purpose: make backend verification fast.

Required information:

- HTTP base URL;
- WebSocket URL;
- current UID/account/device/platform;
- token presence and expiry if available;
- WebSocket connection state;
- last HTTP request status and payload;
- recent inbound WebSocket events;
- raw error messages.

The debug drawer should be collapsible but easy to reopen. It is a core feature
for this validation client, not a temporary afterthought.

## State Model

Recommended store shape:

```text
auth
  profile
  accessToken
  refreshToken
  deviceId
  platform

settings
  httpBaseUrl
  wsUrl

connections
  wsStatus
  lastError

entities
  usersById
  friends
  pendingFriendRequests
  groups
  groupMembersByGroupId

messages
  directByPeerUid
  groupByGroupId
  offlineInbox

debug
  httpEvents
  wsEvents
```

Keep the store simple for the first pass. React state plus small context/hooks
is enough unless complexity grows. Avoid introducing global state libraries
before the data flow actually needs them.

## Development Milestones

Status: Milestones 1-5 are complete for the first Web validation slice.

### Milestone 1: Web Scaffold And Auth

Exit criteria:

- `clients/web` Vite React TypeScript app starts locally.
- Auth screen only exposes account/password and nickname for register.
- Device/platform are generated internally and are not user-facing fields.
- Developer panel can configure HTTP and WebSocket endpoints.
- Register/login work against Gateway.
- Current profile can be loaded through `/api/v1/auth/info`.
- Personal profile editing works through `/api/v1/auth/profile`.
- Session survives refresh through local storage.

### Milestone 2: Direct Message HTTP Validation

Exit criteria:

- User can enter a peer UID and open a direct conversation.
- Direct HTTP send works through `/api/v1/messages/send`.
- History loads through `/api/v1/messages/history`.
- Offline pull works through `/api/v1/messages/offline`.
- Errors are visible in the debug drawer.

### Milestone 3: WebSocket Realtime Validation

Exit criteria:

- WebSocket connects to Gateway with current identity.
- `CMD_SEND_MESSAGE` can be sent from the composer.
- Sender ACK is shown in the chat.
- Receiver online push can be observed by opening two browser sessions.
- Raw inbound/outbound WS events are visible in debug mode.

### Milestone 4: Friends And Groups

Exit criteria:

- Friend request/send/respond/list flows are usable.
- Group create/join/leave/list/member flows are usable.
- Group message send/history flow is usable.
- Group online push can be tested with multiple logged-in sessions.

### Milestone 5: Validation Polish

Exit criteria:

- One README describes how to start backend + web client.
- Common test accounts flow is documented.
- Developer panel clearly distinguishes HTTP send from WebSocket send.
- The normal chat workspace keeps a consumer IM shape and does not expose
  protocol details.
- Known backend/client gaps are listed in the UI docs.

## Future Extension Pool

These items should be recorded as future expansion, not blockers for the first
Web validation client:

- message delete, recall, edit;
- rich media message upload and preview;
- blacklist and privacy rules;
- group owner transfer, admin management, invite, kick, mute;
- read receipts as a first-class UI flow;
- message search;
- notification settings;
- offline push queue visibility;
- multi-device session management;
- Electron shell using the same Web app;
- Qt client using the same HTTP/WS contract and interaction flow.

## Non-Goals For First Pass

- No landing page.
- No production design system.
- No full consumer-grade settings center.
- No mobile-first layout.
- No new backend feature requirement unless the Web client exposes a blocking
  contract bug.
- No hosted deployment requirement.

## Runtime Validation Result

Validation date: 2026-06-08.

Runtime used:

```bash
scripts/dev/run_remote_services_stack.sh
cd clients/web
npm run dev -- --host 127.0.0.1
```

Health checks:

- `http://127.0.0.1:5173/api/v1/health` returned HTTP 200.
- `http://127.0.0.1:8102/api/v1/health` returned HTTP 200.

Latest Playwright two-context test data:

- Alice: `ui-alice-1780926596746`,
  UID `78cd02ae-5c6f-4131-bdd0-0ff46273e8f1`.
- Bob: `ui-bob-1780926596746`,
  UID `0b1e0feb-cfd5-44a7-aded-8f8c1e9441a7`.
- Group: `1339`, `测试群-1780926596746`.

Verified flows:

1. Alice and Bob registered from the UI in separate browser contexts.
2. Each context used separate `sessionStorage` and a separate generated
   `deviceId`.
3. Contact and group rail tabs switched the middle pane away from the message
   list.
4. Alice searched Bob by account, saw Bob's profile card, then sent the friend
   request; Bob saw Alice's nickname in pending requests and accepted.
5. Alice's contact list refreshed, and both users opened the direct chat from
   normal contact UI.
6. Alice sent Bob an HTTP direct message; Bob saw it through online push
   without manual refresh.
7. Alice created group `1339`; Bob joined and opened it from the group list.
8. Alice sent group messages; Bob saw the new group message through online push
   without manual refresh.
9. Group online push carries `sender_uid`, `conversation_type`, and
   `conversation_id` in `PushRequest.body.ext`; Bob saw the new group message
   without manual refresh, and the UI routed the push to the correct group.
10. Rail tab switching was verified: message/contact/group middle panes render
   separately, and the top header does not keep showing a chat target on
   contact or group tabs.
11. Contact search now uses `/api/v1/users/search` and supports account plus
    nickname fuzzy search. The normal UI displays nickname/account/profile
    information and does not expose internal UID values.
12. Group search now uses `/api/v1/groups/search`, and group profile lookup
    uses `/api/v1/groups/info`. Users search first, inspect group info, then
    join. Leave and member list are only exposed inside already joined group
    info.
13. Settings exposes the message send shortcut: Enter sends by default, while
    Ctrl+Enter mode keeps Enter as newline.
14. Incoming direct push while the user is outside the active conversation
    increments the message rail red badge and the corresponding conversation
    unread count; opening the conversation clears it.

Latest local verification commands:

```bash
npm run build
cmake --build build/remote-push-odb --target \
  gateway_server user_server group_server friend_server message_server push_server \
  test_gateway_user_http test_gateway_group_http \
  test_remote_user_client test_remote_group_client -j"$(nproc)"
./build/remote-push-odb/test/gateway_user/test_gateway_user_http
./build/remote-push-odb/test/gateway_group/test_gateway_group_http
./build/remote-push-odb/test/gateway_user/test_remote_user_client
./build/remote-push-odb/test/gateway_group/test_remote_group_client
cmake --build build/remote-push-odb --target test_push_runtime -j"$(nproc)"
./build/remote-push-odb/test/push/test_push_runtime
ctest --test-dir build/remote-push-odb \
  -R '^(PushRuntimeTest|PushGrpcServiceTest|RemotePushNotifierTest|GatewayMessageHttpTest|GatewayMessageWsTest|GatewayGroupMessageHttpTest|FriendGrpcServiceTest|RemoteFriendServiceAdapterTest|GatewayUserHttpTest|RemoteUserServiceAdapterTest)$' \
  --output-on-failure
```

Current push routing contract:

- `im.push.PushRequest.body.ext` contains JSON metadata for `sender_uid`,
  `conversation_type`, and `conversation_id`.
- For direct messages, `conversation_id` is the sender UID from the receiver's
  perspective.
- For group messages, `conversation_id` is the group ID string.
- Sender nickname is still resolved by client-side friend/group history data;
  a future enhancement can include display metadata directly in push payloads.

Current Web UX constraints:

- Normal user flows must remain Chinese and IM-like. Do not reintroduce
  endpoint, platform, device-code, UID, or protocol fields into the login,
  contact, group, settings, or chat primary surfaces.
- Debug-only data, including internal UID and raw HTTP/WS event payloads, must
  stay in the Developer view or right debug drawer.
- Contact and group tabs must render their own middle panes, not the message
  list.
- User add flow is search -> user profile -> friend request.
- Group add flow is search -> group profile -> join. Leave is only available
  from joined group profile.

## Verification Strategy

Manual validation should use at least two browser profiles or two tabs with
separate local storage:

1. Register/login Alice.
2. Register/login Bob.
3. Alice sends friend request to Bob.
4. Bob accepts.
5. Alice sends Bob a direct HTTP message.
6. Bob pulls history/offline messages.
7. Alice and Bob connect through WebSocket.
8. Alice sends Bob a realtime message.
9. Bob receives `CMD_PUSH_MESSAGE`.
10. Alice creates a group.
11. Bob joins the group.
12. Alice sends a group message.
13. Bob receives or loads the group message.

This is the minimum practical validation path for the current backend MVP.
