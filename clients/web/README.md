# MyChat Web Validation Client

This client validates the existing MyChat Gateway HTTP and WebSocket contracts.
It is a desktop-style IM workspace for development, not a production consumer
client.

## Start

One-command local test stack:

```bash
scripts/dev/run_full_stack.sh
```

This starts the full remote-service backend stack and the Vite Web client.
Open:

```text
http://127.0.0.1:5173/
```

Logs are written under `build/dev-runtime/`:

```text
full_stack_backend.log
full_stack_web.log
```

Press `Ctrl+C` in the script terminal to stop both frontend and backend.

Manual backend:

```bash
scripts/dev/prepare_runtime.sh
build/remote-push-odb/gateway/gateway_server --config config/dev.json
```

or run the full remote stack:

```bash
scripts/dev/run_remote_services_stack.sh
```

Web client:

```bash
cd clients/web
npm install
npm run dev
```

Optional one-command overrides:

```bash
MYCHAT_WEB_PORT=5174 scripts/dev/run_full_stack.sh
MYCHAT_DEV_REMOTE_SERVICES_CONFIG=config/dev.remote-all.json scripts/dev/run_full_stack.sh
```

Default endpoints:

```text
HTTP:      same-origin /api through Vite proxy to http://127.0.0.1:8102
WebSocket: same-origin /ws through Vite proxy to wss://127.0.0.1:8101
```

The browser client defaults to same-origin HTTP requests so Vite can proxy
`/api/*` to Gateway without requiring backend CORS headers. If the app is
served outside Vite, set the HTTP base URL in Settings and make sure Gateway
allows that browser origin.

The WebSocket default also goes through the current Vite origin, for example
`ws://127.0.0.1:5173/ws` or `ws://127.0.0.1:5174/ws` when Vite auto-selects a
free port. This avoids browser rejection of Gateway's local self-signed TLS
certificate during development. If connecting directly to Gateway, use
`wss://127.0.0.1:8101` and trust the development certificate in the browser.

## UI Flow

- Before login, the app shows a dedicated auth screen with login/register tabs.
  It only exposes normal account fields: account, password, and nickname for
  registration. Device ID and platform are generated internally.
- After login, the app opens a desktop IM workspace: primary rail,
  conversation list, central chat panel, and optional debug drawer.
- Direct chats are opened from contacts, search results, or the conversation
  list. Normal UI must not ask users to type internal UID values.
- Friend management uses account/nickname search first, then a user profile
  card, then friend request. Search results show nickname and account only.
- Group management uses group name/group-number search first, then a group
  profile card. Join is available from searched group info; leave is available
  only from an already joined group's info.
- Message send shortcut is configurable in Settings: Enter sends by default;
  Ctrl+Enter mode keeps Enter as newline.
- Conversation unread counts are shown as red badges on the message rail item
  and individual conversation rows.
- Debugging remains available, but endpoint overrides, HTTP/WS send-channel
  selection, and protocol events stay behind Debug or the right-side drawer so
  normal chat usage stays primary.

## Current Scope

- Register, login, profile load, and personal profile editing.
- Direct-message HTTP send, history, and offline pull.
- Friend request, respond, list, and pending list.
- Group create, join, leave, list, members, group-message send/history.
- User account/nickname fuzzy search through `/api/v1/users/search`.
- Group search and group info through `/api/v1/groups/search` and
  `/api/v1/groups/info`.
- WebSocket connection state, token-in-query connection, binary
  `CMD_SEND_MESSAGE` frame encode, and `SendMessageResponse` /
  `PushRequest` frame decode.
- Debug drawer for endpoint settings, token/user state, HTTP responses, and
  WebSocket events.

## Runtime Validation

Validated on 2026-06-08 against `scripts/dev/run_remote_services_stack.sh`
plus Vite at `http://127.0.0.1:5173/`.

Latest Playwright validation data:

- Alice: `ui-alice-1780926596746`,
  UID `78cd02ae-5c6f-4131-bdd0-0ff46273e8f1`.
- Bob: `ui-bob-1780926596746`,
  UID `0b1e0feb-cfd5-44a7-aded-8f8c1e9441a7`.
- Group: `1339`, `测试群-1780926596746`.

Validated flows:

- two isolated browser contexts register/login with separate `sessionStorage`
  sessions and separate generated `deviceId` values;
- friend search by account, profile-card friend request, Bob pending request
  with Alice nickname, Bob accept, and both contact lists refresh without
  manual reload;
- contact and group tabs switch the middle pane away from the message list;
- contact/group/settings headers do not keep showing the old chat target;
- HTTP direct message send triggers online push refresh on the receiver without
  manual refresh;
- group create, join, member list, group message send/history;
- group online push routed by `sender_uid`, `conversation_type`, and
  `conversation_id` metadata in `PushRequest.body.ext`;
- rail tab switching: message, contact, and group sidebars render separately;
  top header clears chat status on non-chat tabs.

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
  -R '^(PushRuntimeTest|PushGrpcServiceTest|RemotePushNotifierTest|GatewayMessageHttpTest|GatewayMessageWsTest|GatewayGroupMessageHttpTest|FriendGrpcServiceTest|RemoteFriendClientTest|GatewayUserHttpTest|RemoteUserClientTest)$' \
  --output-on-failure
```

Push routing metadata: `PushRequest.body.ext` now carries JSON metadata for
`sender_uid`, `conversation_type`, and `conversation_id`. The Web client uses
that metadata to refresh the correct direct conversation or group instead of
assuming the currently open chat.

WebSocket envelope encode/decode is isolated in `src/api/protobufEnvelope.ts`.
It mirrors the current backend frame layout:

```text
varint(header_size) + varint(type_name_size) + type_name + header + payload + crc32
```

The implementation uses `protobufjs/minimal` wire readers/writers for the
current `IMHeader`, direct message, push, and base response fields. If backend
proto contracts change, update this adapter before changing UI components.

Additional validation on 2026-06-08:

- Runtime stack was rebuilt and restarted with `scripts/dev/run_remote_services_stack.sh`.
- Real Gateway HTTP checks passed for nickname fuzzy user search, group search,
  and group info:
  user search returned non-empty `users`, group search returned non-empty
  `groups`, joined group info returned `joined=true` and member data.
- Playwright verified the current Web UI at `http://127.0.0.1:5173/`:
  contact tab renders contact/search UI instead of the message list; group tab
  renders group search/profile UI instead of the message list; settings exposes
  send-shortcut selection; incoming push while on Settings increments the
  message red badge.
