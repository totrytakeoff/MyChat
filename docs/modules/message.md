# Message Service 模块说明

## 当前职责

Message Service 负责单聊消息的持久化和状态管理：

- 单聊消息发送。
- 会话历史查询。
- 离线消息拉取。
- delivered/read 状态标记。
- PostgreSQL/ODB 持久化。
- gRPC 服务边界和独立 `message_server` 进程。

在线推送由 Push 模块负责，Message Service 只负责消息数据和状态。

## 对外入口

### Gateway HTTP

```text
POST /api/v1/messages/send
GET  /api/v1/messages/history
GET  /api/v1/messages/offline
```

### Gateway WebSocket

```text
CMD_SEND_MESSAGE + im.message.SendMessageRequest
```

### gRPC

定义在 `common/proto/message.proto`：

```text
im.message.MessageService.SendMessage
im.message.MessageService.GetConversation
im.message.MessageService.PullOffline
im.message.MessageService.MarkDelivered
im.message.MessageService.MarkRead
```

## 核心链路

### HTTP 单聊发送

```text
POST /api/v1/messages/send
-> Gateway 验证 token
-> sender_uid 使用 token uid
-> MessageParser / UnifiedMessage
-> GatewayCommandHandlerRegistry
-> GatewayRuntimeRegistry
-> local MessagePacketDispatcher 或 remote MessageService.ForwardPacket
-> MessageService 持久化
-> 返回 message 和可选 push_event
```

### WebSocket 单聊发送

```text
WebSocket CMD_SEND_MESSAGE
-> MessageParser / UnifiedMessage
-> Gateway 校验 cmd_id、token 和连接身份
-> 使用 token uid 作为 sender_uid
-> GatewayRuntimeRegistry 转发 packet 到 Message Service
-> MessagePacketDispatcher 解析 SendMessageRequest
-> MessageService 持久化
-> 返回 SendMessageResponse ack 和 push_event
-> Gateway 将 push_event 交给 PushNotifier best-effort 推送给接收者
```

### 离线拉取

```text
GET /api/v1/messages/offline
-> 查询当前用户未投递消息
-> 返回消息列表
-> 将返回消息标记 delivered
```

## 关键设计

- 消息先持久化，再触发在线推送。
- 发送 ack 表示服务端已经接受并保存消息，不等价于对方已读。
- Push 失败不回滚消息，消息仍可通过离线拉取补偿。
- sender_uid 由 Gateway token 决定，避免客户端伪造发送者。
- Gateway 不解析 `SendMessageRequest` 业务字段，业务 payload 解析在 Message Service 侧完成。

## 数据与依赖

- PostgreSQL/ODB：保存 `im_messages`。
- Gateway：负责 HTTP/WS 协议和 token 身份。
- Push：负责在线投递。

## 面试可讲点

- 为什么 IM 消息要先落库再推送。
- Push 失败后如何保证消息不丢。
- ack、delivered、read 三类状态的区别。
- 如何防止客户端伪造 sender_uid。
- 当前是 at-least-once/best-effort 思路，后续如何演进到更严格的一致性。

## 后续扩展

- 消息撤回、编辑、删除。
- 消息搜索。
- 富媒体消息。
- 消息序列号和更强顺序保证。
- 已读回执 UI 与多端同步。
