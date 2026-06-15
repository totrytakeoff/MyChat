# Gateway 模块说明

## 当前职责

Gateway 是 MyChat 的统一客户端接入层，负责对外暴露 HTTP 与 WebSocket，屏蔽后端服务的本地/远程部署差异。

当前 Gateway 负责：

- HTTP API 路由注册和响应整形。
- WebSocket 连接接入、token 认证、会话注册和超时关闭。
- 访问令牌验证，信任 token 中的用户身份，不信任客户端传入的 sender_uid。
- 通过 local/remote facade 调用 User、Message、Friend、Group、Push。
- WebSocket `CMD_SEND_MESSAGE` 的消息解析、持久化调用、ack 返回和在线推送触发。
- 对 Web 客户端隐藏后端是本地服务还是远程 gRPC 服务。

Gateway 不应负责：

- 用户账号持久化规则。
- 消息持久化业务规则。
- 好友/群组关系的领域规则。
- Push fanout 策略本身。

这些逻辑由对应服务模块负责。

## HTTP 接口

### Auth/User

注册在 `gateway/gateway_server/gateway_server.cpp`：

```text
POST /api/v1/auth/register
POST /api/v1/auth/login
GET  /api/v1/auth/info
POST /api/v1/auth/profile
GET  /api/v1/users/search
```

对应控制器：

```text
gateway/http/user_http_controller.*
```

### Message

```text
POST /api/v1/messages/send
GET  /api/v1/messages/history
GET  /api/v1/messages/offline
```

对应控制器：

```text
gateway/http/message_http_controller.*
```

### Friend

```text
POST /api/v1/friends/request
POST /api/v1/friends/respond
GET  /api/v1/friends
GET  /api/v1/friends/pending
```

对应控制器：

```text
gateway/http/friend_http_controller.*
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

对应控制器：

```text
gateway/http/group_http_controller.*
gateway/http/group_message_http_controller.*
```

### Health

```text
GET /api/v1/health
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
-> MessageParser / Processor
-> MessageWsHandler::handle_send
-> MessageClient local/remote facade
-> MessageService 持久化
-> SendMessageResponse ack
-> PushNotifier local/remote facade
-> 在线用户收到 PushRequest
```

关键约束：

- sender_uid 来自 Gateway token 验证结果，不信任客户端传入值。
- 发送成功以“消息持久化成功”为主，Push 是 best-effort。
- Push 失败时消息仍可通过离线拉取补偿。

## Local/Remote Facade

Gateway 对每个服务使用客户端 facade：

- `UserClient`
- `MessageClient`
- `FriendClient`
- `GroupClient`
- `PushNotifier`

本地模式调用进程内 service 对象；远程模式调用 gRPC stub。这样 Gateway 的 HTTP/WS 外部契约保持稳定，服务是否拆成独立进程由配置决定。

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
- local/remote facade 如何支持从单进程开发演进到分布式部署。
- WebSocket 连接由 Gateway 持有时，远程 Push 服务为什么需要 callback。
- 为什么 Gateway 只做协议/鉴权/编排，不做核心领域规则。

## 后续扩展

- 多 Gateway 实例下的连接路由和服务发现。
- 更细的 API 错误码和用户友好错误映射。
- 生产 TLS、限流、审计日志、观测指标。
- Gateway 与 Push 之间的连接位置索引。
