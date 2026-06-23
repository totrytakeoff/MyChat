# Gateway Protobuf Packet Boundary 基准约束

日期：2026-06-23

## 1. 背景

Gateway 已从旧 HTTP controller/client 架构回归到统一消息处理架构。当前主链路为：

```text
HTTP / WebSocket
  -> MessageParser
  -> UnifiedMessage
  -> MessageProcessor
  -> command handler
  -> local packet dispatcher 或 remote ForwardPacket RPC
```

本基准文档用于锁定 packet boundary 的最终边界：Gateway 只负责协议入口、路由、连接管理、鉴权和 packet 转发；业务数据解析与业务处理必须下沉到各 service/server。

## 2. 硬性原则

1. Gateway 不处理除鉴权之外的任何业务逻辑。
2. Gateway 不解析业务 payload 字段，不构造 `RegisterRequest`、`SendRequest`、`FriendRequest`、`CreateGroupRequest` 等业务 DTO。
3. Gateway 不维护业务响应 mapper，不把业务 DTO 转 JSON。
4. Gateway 只允许读取统一 envelope 层信息：`IMHeader`、`cmd_id`、`token`、`device_id`、`platform`、`type_name`、payload bytes。
5. 微服务只能拿到统一 packet/envelope，并在 server 内部自行解析、校验、执行业务和生成响应。
6. HTTP 与 WebSocket 只是入口协议不同，进入 `MessageProcessor` 后必须共享同一套 `cmd_id -> handler -> packet forward` 模型。
7. Gateway handler 可以消费 service 明确返回的边界元数据，例如 auth hint、push event，但不能为了得到这些字段而反解析业务响应 payload。
8. remote adapter 只允许承担 transport 适配职责，例如 gRPC stub、deadline、transport 错误映射；不得在 adapter 中重新实现业务字段解析和业务流程编排。
9. 不新增 `GatewayInvocation`、`GatewayServiceInvoker` 这类额外中间抽象层；注册中心只负责按 `cmd_id` 绑定回调。

## 3. 正确链路

HTTP 场景：

```text
HTTP request
  -> MessageParser 解析 route/cmd/token/raw_body
  -> UnifiedMessage
  -> MessageProcessor
  -> command handler 校验 token 并补齐 IMHeader
  -> XxxPacketRequest{header, "application/json", raw_body}
  -> service packet dispatcher / remote ForwardPacket
  -> service 解析 JSON、执行业务、构造兼容 HTTP JSON response
  -> Gateway 原样回写 service response payload
```

WebSocket 场景：

```text
WebSocket frame
  -> ProtobufCodec::decodeEnvelope
  -> MessageProcessor
  -> command handler 校验 token/device 并补齐 sender identity
  -> XxxPacketRequest{header, type_name, payload bytes}
  -> service packet dispatcher / remote ForwardPacket
  -> service 解析业务 protobuf、执行业务、构造 protobuf response
  -> Gateway encode 后回写
```

## 4. Codec 定位

`common/network/ProtobufCodec`、`common/network/PacketErrorBuilder` 与 `common/proto/codec_service.proto` 是统一消息包边界：

- `encode(header, message, output)`：将 `IMHeader + protobuf message` 编码为统一二进制包。
- `decodeEnvelope(input, header, type_name, payload)`：只解 envelope，不解析业务消息。
- `PacketErrorBuilder::build_json_error_response(...)`：只用于 Gateway 本地拦截错误，生成 `application/json` envelope 错误包，不构造任何业务 protobuf response。
- `CodecService` 后续用于跨语言/跨进程复用统一编解码能力。

Gateway 应主要使用 `decodeEnvelope`。只有具体业务 service/server 才能把 payload 解析为具体 protobuf 类型。

## 5. Gateway 侧 packet 类型

当前 Gateway 与 service adapter 不再维护自定义 `ServicePacket` / `ServicePacketResult`。

唯一 packet contract 来自 `common/proto`：

- `im.user.UserPacketRequest` / `im.user.UserPacketResponse`
- `im.message.MessagePacketRequest` / `im.message.MessagePacketResponse`
- `im.friend_.FriendPacketRequest` / `im.friend_.FriendPacketResponse`
- `im.group.GroupPacketRequest` / `im.group.GroupPacketResponse`

Gateway 只能消费这些 protobuf response 中的边界元数据：

- `payload`：原样作为 HTTP/WS 响应主体。
- `base.error_code` / `base.error_message` / `http_status`：用于统一错误码和 HTTP 状态回写。
- `MessagePushEvent` / `GroupPushEvent`：转交给 `PushNotifier`。
- `UserAuthTokenHint`：登录/注册成功后由 Gateway 签发 token 并注入 HTTP 响应。

## 6. 各模块落地状态

### User

已完成：

- `common/proto/user.proto` 增加 `ForwardPacket(UserPacketRequest) returns (UserPacketResponse)`。
- `services/user/user_packet_dispatcher.*` 解析注册、登录、资料查询/更新、用户搜索 payload。
- `gateway/command_handlers/user_command_handlers.cpp` 主链路只做依赖检查、按命令决定是否鉴权、header 补齐、packet 转发、HTTP 响应回写。
- `gateway/service_adapters/remote_user_service_adapter.cpp` 通过 `ForwardPacket` 调用远端 user service。
- `gateway/service_adapters/user_service_adapter.hpp` 只暴露 `forward(UserPacketRequest)`。

边界例外：

- Gateway 仍负责 token 签发。`user_command_handlers.cpp` 中的 JSON parse 只用于向 service 返回的响应 payload 注入 `access_token` / `refresh_token`，不得扩展为用户业务字段解析。

### Message

已完成：

- `common/proto/message.proto` 增加 `ForwardPacket(MessagePacketRequest) returns (MessagePacketResponse)`。
- `services/message/message_packet_dispatcher.*` 解析单聊发送、历史、离线拉取的 HTTP JSON payload，以及发送消息 WebSocket protobuf payload。
- delivered 状态更新已从 Gateway 迁入 message service packet dispatcher。
- message push 字段由 service 返回 `MessagePushEvent`，Gateway 只做转交。
- `gateway/service_adapters/message_service_adapter.hpp` 只暴露 `forward(MessagePacketRequest)`。

### Friend

已完成：

- `common/proto/friend.proto` 增加 `ForwardPacket(FriendPacketRequest) returns (FriendPacketResponse)`。
- `services/friend/friend_packet_dispatcher.*` 解析好友申请、处理申请、好友列表、待处理申请的 HTTP JSON payload。
- Gateway 不再解析 `target_uid`、`friend_id`、`accept` 等字段。
- `gateway/service_adapters/friend_service_adapter.hpp` 只暴露 `forward(FriendPacketRequest)`。

### Group

已完成：

- `common/proto/group.proto` 增加 `ForwardPacket(GroupPacketRequest) returns (GroupPacketResponse)`。
- `services/group/group_packet_dispatcher.*` 解析建群、搜群、群资料、群列表、入群、退群、成员列表、群消息发送、群历史的 HTTP JSON payload。
- 群存在校验、成员校验、群资料组合、群消息 fanout 成员计算已迁入 group service packet dispatcher。
- group push 字段由 service 返回 `GroupPushEvent`，Gateway 只做转交。
- `gateway/service_adapters/group_service_adapter.hpp` 只暴露 `forward(GroupPacketRequest)`。

## 7. 仍需清理点

- `GatewayServer` 初始化与成员生命周期仍依赖 CMake target 和 `IM_ENABLE_*` 宏；当前已完成分层 runtime init、统一配置读取、`service_runtime_` 状态表、`GatewayServer::runtime_registry_` 长生命周期持有，以及 user/message/friend/group service/adapter 所有权迁移。后续还需要继续收敛为更泛化的 runtime service registry / endpoint map。
- `common/proto/user.proto`、`message.proto`、`friend.proto`、`group.proto` 当前同时混放 packet 契约类型和业务类型。长期应拆出 `*_packet.proto` 或统一 `gateway_packet.proto`，让 Gateway 只 include packet contract，避免编译期接触 `SendMessageResponse`、`RegisterRequest` 等业务 proto 类型。
- 统一入口 HTTP API 回归测试已补回 local all-in-one smoke；后续继续扩展异常路径和 WS+HTTP 混合端到端测试。
- `docs/final_sum_docs` 中 controller/facade 相关叙述需要重写为统一消息模型和 packet boundary。

## 8. 验收扫描

Gateway command handler 不得命中业务字段解析、业务 DTO 构造、业务 mapper：

```bash
rg -n 'body\.value\(|parse_json_body|query_value\(|parse_limit\(|RegisterRequest|LoginRequest|UpdateProfileRequest|CreateGroupRequest|group_info_to_json|member_info_to_json|user_profile_to_json|search_users\(|get_user_info\(|update_user|search_groups\(|list_my_groups\(|get_group_info\(|join_group\(|leave_group\(|group_exists\(|list_members\(|send_message\(|get_history\(' \
  gateway/command_handlers/*.cpp
```

Gateway 不应再保留业务 response mapper：

```bash
rg -n 'response_mappers|group_info_to_json|member_info_to_json|profile_to_json|message_data_to_json|friend_info_to_json' \
  gateway -g'*.cpp' -g'*.hpp' -g'CMakeLists.txt'
```

允许的 service 侧 mapper：

```text
services/user/user_packet_dispatcher.cpp
services/message/message_packet_dispatcher.cpp
services/friend/friend_packet_dispatcher.cpp
services/group/group_packet_dispatcher.cpp
```

## 9. 验证命令

低并发构建，避免本地 OOM：

```bash
cmake --build /tmp/mychat-build-gcc-odb --target gateway_server -j2
cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target gateway_server test_local_gateway_http_smoke -j2
cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target \
  test_user_packet_dispatcher test_user_service test_user_grpc_service test_remote_user_adapter \
  test_group_service test_group_packet_dispatcher test_group_grpc_service test_remote_group_adapter test_remote_group_gateway_server_smoke \
  -j2
ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests \
  -R 'Remote(User|Message|Friend|Group)GatewayServerSmokeTest|RemotePushGateway(Entrypoints|ServerSmoke)Test|LocalGatewayHttpSmokeTest' \
  --output-on-failure
ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests \
  -R 'User(ServiceCore|PacketDispatcher|GrpcService)Test|RemoteUserServiceAdapterTest|Group(ServiceCore|PacketDispatcher|GrpcService)Test|RemoteGroup(ServiceAdapter|GatewayServerSmoke)Test' \
  --output-on-failure
```

## 10. 后续约束

- 不改前端 URL。
- 不改数据库 schema。
- 不恢复旧 controller 入口。
- 不恢复 local thin client。
- 不把 token 生成迁入 user service。
- 不让 Gateway 重新解析 user/message/friend/group 业务字段。
- remote adapter 只做 packet RPC 适配，不做业务编排。
