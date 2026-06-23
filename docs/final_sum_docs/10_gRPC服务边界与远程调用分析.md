# gRPC 服务边界与远程调用分析

日期：2026-06-23

## 1. 文档目标

这篇文档解释 MyChat 当前的分布式服务边界。

当前不要再把 gRPC 边界讲成旧的：

```text
Gateway Controller -> Local*Client / Remote*Client -> Service
```

当前权威模型是：

```text
Gateway command handler
-> GatewayRuntimeRegistry
-> local service packet dispatcher
或
-> remote service adapter
-> XxxService::ForwardPacket(XxxPacketRequest)
-> service server packet dispatcher
```

一句话概括：

```text
MyChat 用统一 packet 契约把 Gateway 外部 HTTP/WS 协议和内部服务部署方式解耦；
local 模式直接调用 service packet dispatcher，remote 模式通过 gRPC ForwardPacket 到独立服务进程，
两种模式共享同一套业务解析和业务规则。
```

## 2. 为什么用 ForwardPacket

如果 Gateway 直接调用一堆业务 RPC，比如：

```text
SendMessage
GetConversation
CreateGroup
JoinGroup
SendRequest
```

Gateway 就需要：

- 解析业务 payload。
- 构造业务 request DTO。
- 映射业务 response。
- 为每个服务维护一套 local/remote 业务方法。

这会让 Gateway 重新变成业务编排层，违背当前边界。

当前 `ForwardPacket` 的目标是：

```text
Gateway 只认识统一 packet，不认识业务 payload。
Service 自己解析 payload、执行业务、构造响应。
```

## 3. 当前统一模型

HTTP：

```text
HTTP request
-> MessageParser
-> UnifiedMessage
-> command handler
-> XxxPacketRequest(type_name=application/json, payload=raw json)
-> local dispatcher 或 remote ForwardPacket
-> XxxPacketResponse(payload=json, http_status)
```

WebSocket：

```text
WebSocket envelope
-> MessageParser
-> UnifiedMessage
-> command handler / MessageWsHandler
-> XxxPacketRequest(type_name=protobuf type, payload=protobuf bytes)
-> local dispatcher 或 remote ForwardPacket
-> XxxPacketResponse(type_name=protobuf response type, payload=protobuf bytes)
-> Gateway encode envelope
```

## 4. GatewayRuntimeRegistry

`GatewayRuntimeRegistry` 是 Gateway 当前的运行时依赖表。

它保存：

- local `UserService` 或 remote `UserServiceAdapter`。
- local `MessageService` 或 remote `MessageServiceAdapter`。
- local `FriendService` 或 remote `FriendServiceAdapter`。
- local `GroupService/GroupMessageService` 或 remote `GroupServiceAdapter`。
- auth manager。
- push notifier。
- message ws handler。

handler 只拿一个 `GatewayCommandHandlerContext`：

```cpp
struct GatewayCommandHandlerContext {
    const GatewayRuntimeRegistry* runtime = nullptr;
};
```

这比旧的展开式 service/client 成员更清晰，也减少了 handler 注册函数的参数爆炸。

## 5. User Service 边界

proto：

```text
common/proto/user.proto
```

packet RPC：

```proto
rpc ForwardPacket(UserPacketRequest) returns (UserPacketResponse);
```

当前 User packet 覆盖：

- `CMD_REGISTER`
- `CMD_LOGIN`
- `CMD_GET_USER_INFO`
- `CMD_UPDATE_USER_INFO`
- `CMD_SEARCH_USER`

remote 链路：

```text
user_command_handlers.cpp
-> RemoteUserServiceAdapter::forward
-> UserService::ForwardPacket
-> user_grpc_service.cpp
-> UserPacketDispatcher::handle
-> UserService
```

特殊边界：

- User service 返回 `UserAuthTokenHint`。
- Gateway 根据 hint 生成 access/refresh token。
- token 签发仍属于 Gateway 鉴权边界。

## 6. Message Service 边界

proto：

```text
common/proto/message.proto
```

packet RPC：

```proto
rpc ForwardPacket(MessagePacketRequest) returns (MessagePacketResponse);
```

当前 Message packet 覆盖：

- `CMD_SEND_MESSAGE`
- `CMD_MESSAGE_HISTORY`
- `CMD_PULL_MESSAGE`

remote 链路：

```text
message_command_handlers.cpp 或 MessageWsHandler
-> RemoteMessageServiceAdapter::forward
-> MessageService::ForwardPacket
-> message_grpc_service.cpp
-> MessagePacketDispatcher::handle
-> MessageService
```

特殊边界：

- HTTP payload 是 JSON。
- WS payload 是 `im.message.SendMessageRequest` protobuf bytes。
- Message service 返回 `MessagePushEvent`。
- Gateway 只消费 `push_event` 并转交 PushNotifier。

## 7. Friend Service 边界

proto：

```text
common/proto/friend.proto
```

packet RPC：

```proto
rpc ForwardPacket(FriendPacketRequest) returns (FriendPacketResponse);
```

当前 Friend packet 覆盖：

- `CMD_ADD_FRIEND`
- `CMD_HANDLE_FRIEND_REQUEST`
- `CMD_GET_FRIEND_LIST`
- `CMD_GET_FRIEND_REQUESTS`

remote 链路：

```text
friend_command_handlers.cpp
-> RemoteFriendServiceAdapter::forward
-> FriendService::ForwardPacket
-> friend_grpc_service.cpp
-> FriendPacketDispatcher::handle
-> FriendService
```

## 8. Group Service 边界

proto：

```text
common/proto/group.proto
```

packet RPC：

```proto
rpc ForwardPacket(GroupPacketRequest) returns (GroupPacketResponse);
```

当前 Group packet 覆盖：

- `CMD_CREATE_GROUP`
- `CMD_SEARCH_GROUP`
- `CMD_GET_GROUP_INFO`
- `CMD_GET_GROUP_LIST`
- `CMD_APPLY_JOIN_GROUP`
- `CMD_QUIT_GROUP`
- `CMD_GET_GROUP_MEMBERS`
- `CMD_SEND_GROUP_MESSAGE`
- `CMD_GET_GROUP_MESSAGES`

remote 链路：

```text
group_command_handlers.cpp
-> RemoteGroupServiceAdapter::forward
-> GroupService::ForwardPacket
-> group_grpc_service.cpp
-> GroupPacketDispatcher::handle
-> GroupService / GroupMessageService
```

特殊边界：

- Group service 返回 `repeated GroupPushEvent push_events`。
- Gateway 不计算群成员 fanout。
- Gateway 只遍历 push_events 并调用 PushNotifier。

## 9. Push Service 边界

Push 和其他业务服务不同，因为 WebSocket session 在 Gateway 进程内。

remote push 链路：

```text
Gateway
-> RemotePushNotifier
-> push_server PushService.NotifyUser
-> PushRuntime
-> GatewayPushDeliveryService.ListUserSessions
-> GatewayPushDeliveryService.SendSessionPayload
-> GatewayPushDeliveryService.MarkMessageDelivered
-> Gateway session lookup / payload send
```

为什么需要 Gateway callback：

```text
远程 push_server 无法直接访问 Gateway 内存中的 WebSocket session map。
所以它只能通过 Gateway 暴露的 delivery service 回调完成实际投递。
```

这是项目里最典型的分布式边界取舍。

## 10. local 模式

local 模式：

```text
Gateway command handler
-> GatewayRuntimeRegistry local service pointer
-> XxxPacketDispatcher::handle(packet)
-> Service core
-> Repository / Redis / Push runtime
```

优点：

- 启动简单。
- 本地调试方便。
- 不需要多个服务进程。
- 适合单机 MVP 验证和快速开发。

## 11. remote 模式

remote 模式：

```text
Gateway command handler
-> RemoteXxxServiceAdapter::forward(packet)
-> gRPC ForwardPacket
-> standalone service process
-> XxxPacketDispatcher::handle(packet)
-> Service core
```

优点：

- 更接近真实分布式服务。
- 可以把 User/Message/Friend/Group/Push 分进程部署。
- 可以验证 protobuf、gRPC deadline、服务不可用、网络错误等边界。
- Gateway 对外 HTTP/WS 协议不变。

代价：

- 启动和排查复杂度更高。
- 需要 endpoint 配置。
- 需要处理 gRPC 超时和 transport error。
- 多进程日志和数据一致性排查更复杂。

## 12. Remote Service Adapter 的职责

remote adapter 当前职责很窄：

- 接收 `XxxPacketRequest`。
- 设置 gRPC deadline。
- 调用 `ForwardPacket`。
- 检查 transport status。
- 返回 `XxxPacketResponse`。

remote adapter 不应该：

- 暴露 `send_text_message`、`create_group`、`send_request` 等业务方法。
- 解析业务 JSON/protobuf payload。
- 构造业务 DTO。
- 做业务错误 mapper 之外的业务逻辑。

## 13. proto 当前状态

当前 `user.proto/message.proto/friend.proto/group.proto` 同时包含：

- packet contract：`XxxPacketRequest/Response`。
- 历史业务 RPC：`Register`、`SendMessage`、`CreateGroup` 等。
- 业务 DTO：`RegisterRequest`、`MessageBody`、`GroupInfo` 等。

当前 Gateway 主链路只应依赖 packet contract。长期优化方向：

```text
拆出 *_packet.proto 或统一 gateway_packet.proto，
让 Gateway 编译期只接触 packet contract，
不接触业务 request/response DTO。
```

## 14. remote-all 验证

remote-all 模式目标：

```text
Gateway
-> remote UserService
-> remote MessageService
-> remote FriendService
-> remote GroupService
-> remote PushService
```

配置：

- `config/dev.remote-all.json`

相关服务进程：

- `user_server`
- `message_server`
- `friend_server`
- `group_server`
- `push_server`
- `gateway_server`

验证重点：

- 注册/登录 token 签发是否正常。
- HTTP route 是否都能通过 Gateway 进入 remote `ForwardPacket`。
- WS 单聊是否能通过 remote message service。
- 群消息是否能通过 remote group service 返回 push_events。
- remote push callback 是否能回到 Gateway 投递 WebSocket session。

## 15. 源码导读

| 服务 | proto | Gateway remote adapter | service gRPC |
| --- | --- | --- | --- |
| User | `common/proto/user.proto` | `gateway/service_adapters/remote_user_service_adapter.cpp` | `services/user/user_grpc_service.cpp` |
| Message | `common/proto/message.proto` | `gateway/service_adapters/remote_message_service_adapter.cpp` | `services/message/message_grpc_service.cpp` |
| Friend | `common/proto/friend.proto` | `gateway/service_adapters/remote_friend_service_adapter.cpp` | `services/friend/friend_grpc_service.cpp` |
| Group | `common/proto/group.proto` | `gateway/service_adapters/remote_group_service_adapter.cpp` | `services/group/group_grpc_service.cpp` |
| Push | `common/proto/push.proto` | `gateway/push/remote_push_notifier.cpp` | `services/push/push_grpc_service.cpp` |

核心代码：

- `gateway/gateway_server/gateway_server.cpp`
  - `init_user_runtime`
  - `init_message_runtime`
  - `init_friend_runtime`
  - `init_group_runtime`
  - `init_message_ws_runtime`
  - `refresh_runtime_registry`
- `gateway/command_handlers/*_command_handlers.cpp`
- `gateway/service_adapters/remote_*_service_adapter.cpp`
- `services/*/*_grpc_service.cpp`
- `services/*/*_packet_dispatcher.cpp`

## 16. 面试追问

### Q: 为什么不让 Gateway 直接调用业务 RPC？

因为直接调用业务 RPC 会迫使 Gateway 解析 payload、构造业务 DTO、维护 response mapper。当前项目希望 Gateway 只做协议入口和 packet 转发，所以统一使用 `ForwardPacket` 把业务解析下沉到 service。

### Q: local 和 remote 怎么保证行为一致？

local 和 remote 都使用同一个 `XxxPacketRequest/XxxPacketResponse` 契约，并最终进入同一个 `XxxPacketDispatcher::handle`。区别只是 dispatcher 在 Gateway 进程内执行，还是在远程 service 进程内执行。

### Q: 当前 gRPC 边界还不完美在哪里？

packet contract 和业务 proto 仍混在同一个文件里，Gateway 编译期仍可能接触业务 DTO。后续应拆出独立 packet proto，让 Gateway 只依赖 packet contract。

### Q: Push 为什么不是简单 remote ForwardPacket？

因为 Push 的实际投递对象是 Gateway 内存里的 WebSocket session。远程 Push 只能调度投递，最终发送 payload 必须通过 Gateway callback 回到 Gateway 完成。
