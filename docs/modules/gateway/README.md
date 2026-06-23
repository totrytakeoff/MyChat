# Gateway 模块说明

## 当前职责

Gateway 是 MyChat 的统一客户端接入层，负责对外暴露 HTTP 与 WebSocket，屏蔽后端服务的本地/远程部署差异。

当前 Gateway 负责：

- HTTP API 统一路由、命令解析和统一 packet 响应回写。
- WebSocket 连接接入、token 认证、会话注册和超时关闭。
- 访问令牌验证，信任 token 中的用户身份，不信任客户端传入的 sender_uid。
- 按 local/remote 模式将 `XxxPacketRequest` 转发到本地 service packet dispatcher 或远程 `ForwardPacket` gRPC。
- WebSocket `CMD_SEND_MESSAGE` 的 token/device 校验、packet 转发、ack 回写和在线推送触发。
- 对 Web 客户端隐藏后端是本地服务还是远程 gRPC 服务。

Gateway 不应负责：

- 用户账号持久化规则。
- 消息持久化业务规则。
- 好友/群组关系的领域规则。
- 业务 payload 字段解析和业务 DTO 构造。
- 业务 DTO 到 JSON 的响应 mapper。
- Push fanout 策略本身。

这些逻辑由对应服务模块负责。

## HTTP 接口

### Auth/User

这些业务接口不再注册为模块级显式路由，而是由
`GatewayServer::init_http_server()` 的 HTTP catch-all 统一进入：

```text
HTTP request
-> MessageParser::parse_http_request_enhanced
-> UnifiedMessage
-> MessageProcessor::process_message
-> GatewayCommandHandlerRegistry
-> service adapter
```

路由映射来自 `config/*.json` 的 `http_router.routes`。

### Auth/User

```text
POST /api/v1/auth/register
POST /api/v1/auth/login
GET  /api/v1/auth/info
POST /api/v1/auth/profile
GET  /api/v1/users/search
```

对应命令：

```text
CMD_REGISTER
CMD_LOGIN
CMD_GET_USER_INFO
CMD_UPDATE_USER_INFO
CMD_SEARCH_USER
```

### Message

```text
POST /api/v1/messages/send
GET  /api/v1/messages/history
GET  /api/v1/messages/offline
```

对应命令：

```text
CMD_SEND_MESSAGE
CMD_MESSAGE_HISTORY
CMD_PULL_MESSAGE
```

### Friend

```text
POST /api/v1/friends/request
POST /api/v1/friends/respond
GET  /api/v1/friends
GET  /api/v1/friends/pending
```

对应命令：

```text
CMD_ADD_FRIEND
CMD_HANDLE_FRIEND_REQUEST
CMD_GET_FRIEND_LIST
CMD_GET_FRIEND_REQUESTS
```

### Group

```text
POST /api/v1/groups
POST /api/v1/groups/join
POST /api/v1/groups/leave
GET  /api/v1/groups/info
GET  /api/v1/groups/search
GET  /api/v1/groups
GET  /api/v1/groups/members
POST /api/v1/groups/messages/send
GET  /api/v1/groups/messages/history
```

对应命令：

```text
CMD_CREATE_GROUP
CMD_APPLY_JOIN_GROUP
CMD_QUIT_GROUP
CMD_GET_GROUP_INFO
CMD_SEARCH_GROUP
CMD_GET_GROUP_LIST
CMD_GET_GROUP_MEMBERS
CMD_SEND_GROUP_MESSAGE
CMD_GET_GROUP_MESSAGES
```

### Health / Stats

```text
GET /api/v1/health
GET /api/v1/stats
```

## WebSocket 链路

当前核心命令：

```text
CMD_SEND_MESSAGE -> im.message.SendMessageRequest
CMD_PUSH_MESSAGE -> im.push.PushRequest
```

发送链路：

```text
WebSocket binary frame
-> MessageParser / MessageProcessor
-> MessageWsHandler::handle_send
-> MessagePacketRequest
-> MessagePacketDispatcher / RemoteMessageServiceAdapter::forward
-> Message service 解析业务 protobuf 并持久化
-> MessagePacketResponse ack
-> PushNotifier
-> 在线用户收到 PushRequest
```

关键约束：

- sender_uid 来自 Gateway token 验证结果，不信任客户端传入值。
- 发送成功以“消息持久化成功”为主，Push 是 best-effort。
- Push 失败时消息仍可通过离线拉取补偿。

## WebSocket 调度与流控

当前 WSS 业务消息采用受控线程池调度：

```text
WSS binary frame
-> MessageParser::parse_websocket_message_enhanced
-> GatewayServer inflight limit check
-> ThreadPool::Enqueue(one business task)
-> MessageProcessor::process_message_sync
-> MessageWsHandler::handle_send
-> SendMessageResponse / error protobuf response
```

核心约束：

- 正常 WSS 消息路径不再为每条消息创建 `std::async` 独立任务和
  `std::thread(...).detach()` 等待线程。
- `MessageProcessor::process_message()` 是异步入口，统一投递到全局
  `ThreadPool`。
- `MessageProcessor::process_message_sync()` 是同步处理核心，供已经处在
  受控执行器中的 WSS 路径直接调用。
- `gateway.max_ws_inflight_messages` 控制 WSS 排队中和执行中的消息数量，
  默认值为 `4096`。
- 超过 inflight 上限时，Gateway 直接返回 overload 响应：
  `Gateway is busy, please retry later.`。
- 认证失败和认证超时的延迟关闭通过 `schedule_delayed_close()` 投递到
  线程池，不再创建 detached 线程。

Token 校验职责：

- HTTP 消息由通用 `MessageProcessor` 做统一 token 校验。
- WebSocket 消息由具体 WS handler 校验。当前
  `MessageWsHandler::handle_send()` 会校验 token，并从 token 中推导真实
  sender。

可观测统计：

```text
ws.inflight_messages
ws.max_inflight_messages
thread_pool.threads
thread_pool.queued_or_running_tasks
```

当前流控是 inflight cap，还不是独立业务 executor 的严格 bounded queue。
后续压测专项可以继续拆出 Gateway 专用 executor、拒绝计数、处理耗时分位数。

## Local/Remote Service Adapter

Gateway 对每个服务使用 local service / remote adapter 边界：

- local 模式：handler 补齐 header 后调用 `UserPacketDispatcher`、`MessagePacketDispatcher`、`FriendPacketDispatcher`、`GroupPacketDispatcher`。
- remote 模式：通过 `RemoteUserServiceAdapter`、`RemoteMessageServiceAdapter`、`RemoteFriendServiceAdapter`、`RemoteGroupServiceAdapter` 调用远程 gRPC `ForwardPacket`。
- `PushNotifier` 负责屏蔽本地 Push 与远程 PushNotifier 差异。

这样 Gateway 的 HTTP/WS 外部契约保持稳定，服务是否拆成独立进程由配置决定。

当前 service adapter 已位于 `gateway/service_adapters/`；`gateway/http/` 只保留 HTTP 协议辅助类型。

启动层当前状态：

- `GatewayServer::init_server()` 已按数据库、user/message/friend/group/group-message、WS/push runtime 拆分为独立初始化方法。
- `user.mode`、`message.mode`、`friend.mode`、`group.mode`、`push.mode` 的读取已统一为 service runtime config helper。
- `GatewayServer::service_runtime_` 统一记录各服务 mode、endpoint、timeout、local/remote bound 状态，并通过 `/api/v1/stats` 输出。
- `GatewayServer::runtime_registry_` 已承载 handler 注册所需的运行时依赖：user/message/friend/group 的 local service 与 remote adapter 由 registry 持有，push/ws/auth 以生命周期受控的裸指针形式挂入 registry。
- `GatewayCommandHandlerContext` 已收敛为单一 `runtime` 指针，handler 不再从 context 中展开各模块 service/adapter，也不再维护第二份 `auth_mgr` 来源。
- 后续目标是继续收敛为更泛化的 runtime service registry / endpoint map，进一步减少 `GatewayServer` 对具体服务类型和 `IM_ENABLE_*` 宏的硬编码。

典型配置项：

```text
user.mode
message.mode
friend.mode
group.mode
push.mode
```

## 面试可讲点

- Gateway 为什么是统一入口，而不是前端直连每个服务。
- HTTP 与 WebSocket 为什么都放在 Gateway。
- local/remote service adapter 如何支持从单进程开发演进到分布式部署。
- WebSocket 连接由 Gateway 持有时，远程 Push 服务为什么需要 callback。
- 为什么 Gateway 只做协议/鉴权/编排，不做核心领域规则。

## 后续扩展

- 多 Gateway 实例下的连接路由和服务发现。
- 更细的 API 错误码和用户友好错误映射。
- 生产 TLS、限流、审计日志、观测指标。
- Gateway 与 Push 之间的连接位置索引。
