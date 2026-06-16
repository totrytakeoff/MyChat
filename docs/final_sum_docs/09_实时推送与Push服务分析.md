# MyChat 实时推送与 Push 服务分析

日期：2026-06-16

## 1. 文档目标

这份文档专门分析 MyChat 的在线实时推送设计：

- Push 为什么从 Message/Group 中拆出来。
- 本地 Push 和远程 Push 的区别。
- Gateway 为什么仍然持有 WebSocket session。
- PushRuntime、FanoutPolicy、Gateway callback 如何协作。
- Push 的 best-effort 语义和 delivered 标记边界。
- 多 Gateway 生产化还需要补什么。

一句话概括：

```text
MyChat 的 Push 服务负责“在线投递编排”，但不拥有 WebSocket 连接；
Gateway 持有连接，Push 通过本地适配或远程 Gateway callback 完成 session 查询、payload 发送和 delivered 标记。
```

## 2. 为什么需要 Push Service

如果 Message Service 在保存消息后直接调用 WebSocket send，会产生几个问题：

- Message Service 需要知道 Gateway 连接管理细节。
- 群消息 fanout 和单聊推送逻辑会分散在多个模块。
- 多端策略难以统一，例如全部设备、指定平台、最新设备。
- 远程服务拆分后，Message Service 无法直接访问 Gateway 内存 session。

因此 MyChat 把在线投递抽象为 Push：

```text
业务入口只负责“有消息需要通知某用户”
PushRuntime 负责“查哪些 session、发什么 payload、如何标记 delivered”
Gateway 负责“实际 WebSocket session 的所有权和发送”
```

## 3. 模块职责

| 模块 | 职责 |
| --- | --- |
| Gateway Message/Group entrypoint | 在消息持久化成功后触发 PushNotifier |
| PushNotifier | 推送调用抽象，本地/远程都实现同一接口 |
| PushRuntime | 查询 session、应用 fanout policy、构造 payload、发送、标记 delivered |
| FanoutPolicy | 选择要投递的 session 子集 |
| ConnectionManager | Gateway 内部维护 uid -> sessions |
| WebSocketServer | Gateway 内部按 session_id 发送 payload |
| GatewayPushDeliveryService | 远程 Push 回调 Gateway 的 gRPC 服务 |
| MessageClient | 标记单聊消息 delivered |

## 4. Push 触发点

### 4.1 单聊

```text
MessageService 持久化成功
-> MessageHttpController / MessageWsHandler
-> PushNotifier::notify_user(receiver_uid, msg_id, content, context)
```

### 4.2 群聊

```text
GroupMessageService 持久化成功
-> Gateway 查询群成员
-> 对除发送者外的成员逐个 PushNotifier::notify_user(member_uid, msg_id, content, context)
```

群聊中 `context.conversation_type = "group"`，`conversation_id = group_id`。

单聊中 `context.conversation_type = "direct"`。

## 5. 本地 Push 模式

本地模式中 PushService 在 Gateway 进程内：

```text
Gateway entrypoint
-> gateway/push/PushService
-> services/push/PushRuntime
-> PushService::get_sessions
-> ConnectionManager::get_user_sessions
-> FanoutPolicy::select_sessions
-> PushRuntime::build_payload
-> PushService::send_payload
-> WebSocketServer::get_session
-> session->send(payload)
-> PushService::mark_delivered
-> MessageClient::mark_delivered
```

优点：

- 调试简单。
- 调用链短。
- 适合单机 MVP。

缺点：

- Push 和 Gateway 仍在同一进程。
- 无法单独扩展 Push。

## 6. 远程 Push 模式

远程模式中 Push 是独立进程：

```text
Gateway entrypoint
-> RemotePushNotifier
-> im.push.PushService.NotifyUser
-> push_server PushGrpcService
-> PushRuntime
-> GatewayPushDeliveryService.ListUserSessions
-> GatewayPushDeliveryService.SendSessionPayload
-> GatewayPushDeliveryService.MarkMessageDelivered
```

关键点：

```text
远程 Push 不直接访问 Gateway 内存，而是通过 Gateway callback 完成投递。
```

这保证：

- WebSocket session 所有权仍在 Gateway。
- Push 可以作为独立服务部署。
- Gateway 可以控制 session 查询、payload 发送和 delivered 回写。

## 7. Gateway callback 的必要性

WebSocket 连接是长连接，通常绑定到接入层实例内存。

远程 Push 无法直接拿到：

- `ConnectionManager`
- `WebSocketServer`
- `session` 对象

如果让 Push 持有连接，会导致：

- Gateway 和 Push 职责混乱。
- 客户端连接入口不统一。
- 多 Gateway 场景下连接路由更混乱。

所以当前方案是：

```text
Gateway owns connection.
Push owns delivery decision.
Gateway callback bridges the two.
```

面试中这是一个很好的分布式边界案例。

## 8. PushRuntime 内部流程

`PushRuntime::notify_user` 核心步骤：

```text
1. 检查 session_provider / payload_sender / delivery_marker / fanout_policy 是否存在
2. session_provider->get_sessions(receiver_uid)
3. 如果没有在线 session，返回，消息保持未 delivered
4. fanout_policy->select_sessions(sessions)
5. build_payload(receiver_uid, msg_id, content, context)
6. 对选中 session 调用 payload_sender->send_payload
7. 统计 success_count
8. success_count > 0 时 delivery_marker->mark_delivered(msg_id, now_ms)
```

当前 payload 是：

```text
CMD_PUSH_MESSAGE + im.push.PushRequest
```

PushRequest 中包含：

- content。
- related_message_id。
- ext：sender_uid、conversation_type、conversation_id 等上下文。

## 9. FanoutPolicy

当前策略：

| 策略 | 语义 |
| --- | --- |
| AllSessionsFanoutPolicy | 推送给用户所有在线 session |
| PlatformFilterFanoutPolicy | 只推送给指定平台 |
| NewestSessionFanoutPolicy | 只推送给最新连接 session |

默认是：

```text
AllSessionsFanoutPolicy
```

适合当前多窗口、多端验证。

面试表达：

```text
PushRuntime 不直接写死投递到哪些设备，而是通过 FanoutPolicy 把多端策略抽象出来。
当前默认全端推送，后续可以按平台、最新设备、用户设置等策略扩展。
```

## 10. delivered 标记语义

当前 delivered 标记发生在：

```text
至少一个 session 成功接受 push payload 后
```

这意味着：

- delivered 不等于所有设备都收到。
- delivered 不等于用户已读。
- delivered 表示服务端已经完成至少一次在线投递，或离线拉取时补标。

这和 read 的区别：

```text
delivered = 送达设备或被拉取
read = 用户真正读过
```

## 11. Push 的 best-effort 语义

Push 当前是 best-effort。

这意味着：

- 没有在线 session 时不报致命错误。
- session 发送失败不回滚消息。
- 远程 Push RPC 失败不影响已持久化消息。
- Push 成功只是提升实时性。

消息可靠性由持久化和离线拉取兜底：

```text
先持久化 -> 再 Push -> Push 失败 -> offline/history 补偿
```

## 12. 当前优势

- Push 从 Message/Group 中抽象出来。
- 支持本地和远程 Push。
- 支持 fanout policy。
- 远程 Push 不破坏 Gateway 的 WebSocket session 所有权。
- 单聊和群聊可复用同一套 PushRuntime。
- delivered 标记与 Push 成功关联。

## 13. 当前不足和后续优化

当前不足：

- 多 Gateway 下还没有全局连接位置注册表。
- Push 没有持久投递日志。
- 没有离线推送队列和重试诊断。
- delivered 是“至少一个 session 成功”，还不是多设备送达明细。
- 用户免打扰、平台过滤等设置还没有完整持久化。
- Push 指标和 trace 还不完整。

后续优化：

- Redis/Etcd 维护 uid -> gateway_id/session_id 连接索引。
- Push 请求异步入队，避免 Gateway 同步等待 fanout。
- 增加 push_delivery_log。
- 增加 per-device delivered 状态。
- 用户通知设置持久化。
- 增加 Push QPS、失败率、投递延迟监控。
- 多 Gateway 下按连接归属回调对应 Gateway。

## 14. 面试问答准备

### 14.1 为什么 Push 不直接写在 Message Service 里

回答思路：

```text
Message Service 负责消息数据和状态，Push 负责在线投递。
如果 Message 直接操作 WebSocket，会耦合 Gateway 连接管理，也不利于群聊、多端策略和远程部署。
```

### 14.2 远程 Push 为什么需要 Gateway callback

回答思路：

```text
WebSocket session 在 Gateway 内存里，远程 Push 进程无法直接访问。
所以 Push 只做投递决策，通过 Gateway callback 让 Gateway 查询 session 并发送 payload。
```

### 14.3 Push 失败消息会丢吗

回答思路：

```text
不会因为 Push 失败而丢。消息已经先持久化，Push 是 best-effort。
接收者后续可以通过离线拉取或历史查询拿到。
```

### 14.4 多 Gateway 下如何扩展

回答思路：

```text
需要维护全局连接位置索引，例如 uid -> gateway_id/session_id。
Push 先查用户在哪些 Gateway 上在线，再回调对应 Gateway 投递。
```

### 14.5 FanoutPolicy 有什么意义

回答思路：

```text
它把多端投递策略从 PushRuntime 中抽象出来。当前可以全端推送、按平台过滤、只推最新设备，后续还能接用户通知设置。
```

## 15. 一句话总结

```text
MyChat 的 Push 设计把“消息持久化”和“在线投递”分离：
Message/Group 负责先保存消息，PushRuntime 负责 best-effort fanout，
Gateway 始终持有 WebSocket session，远程 Push 通过 callback 完成跨进程投递。
```

## 附录：源码导读

### A. 源码阅读地图

| 层级 | 文件 | 重点函数/对象 |
| --- | --- | --- |
| Push 抽象 | `services/push/push_notifier.hpp` | `PushNotifier`、`PushContext` |
| Push runtime | `services/push/push_runtime.cpp` | `PushRuntime::notify_user`、`build_payload` |
| Fanout | `services/push/fanout_policy.cpp` | `AllSessionsFanoutPolicy`、`PlatformFilterFanoutPolicy`、`NewestSessionFanoutPolicy` |
| Gateway 本地 Push | `gateway/push/push_service.cpp` | `get_sessions`、`send_payload`、`mark_delivered` |
| 远程 Push client | `gateway/push/remote_push_notifier.cpp` | `RemotePushNotifier::notify_user` |
| Push gRPC server | `services/push/push_grpc_service.cpp` | `NotifyUser` |
| Gateway callback | `gateway/push/gateway_push_delivery_service.cpp` | `ListUserSessions`、`SendSessionPayload`、`MarkMessageDelivered` |
| 协议契约 | `common/proto/push.proto` | `PushService`、`GatewayPushDeliveryService` |

### B. Push 触发点按代码读

单聊 HTTP：

```text
MessageHttpController::handle_send
-> msg_client_->send_text_message 成功
-> PushContext { sender_uid, conversation_type="direct", conversation_id=... }
-> push_notifier_->notify_user(receiver_uid, msg_id, content, context)
```

单聊 WS：

```text
MessageWsHandler::handle_send
-> msg_client_->send_text_message 成功
-> 构造 SendMessageResponse ack
-> push_notifier_->notify_user(receiver_uid, msg_id, content, context)
```

群聊：

```text
GroupMessageHttpController::handle_send_message
-> group_client_->send_message 成功
-> for member in members
-> if member.user_uid != sender
-> push_notifier_->notify_user(member.user_uid, msg_id, content, group context)
```

结论：

```text
Push 都发生在持久化成功之后。
```

### C. PushRuntime 按代码读

```text
PushRuntime::notify_user
-> 检查 session_provider/payload_sender/delivery_marker/fanout_policy
-> sessions = session_provider_->get_sessions(receiver_uid)
-> sessions.empty() 则返回，消息保持未 delivered
-> selected = fanout_policy_->select_sessions(sessions)
-> payload = build_payload(receiver_uid, msg_id, content, context)
-> for session_id in selected:
     payload_sender_->send_payload(session_id, payload)
-> success_count > 0:
     delivery_marker_->mark_delivered(msg_id, now_ms())
```

关键代码点：

- 没有在线 session 不算致命错误。
- 每个 session 发送异常单独 catch，不中断整体流程。
- 至少一个 session 成功才标记 delivered。
- PushRuntime 不知道 Gateway 的具体类，只依赖接口：session provider、payload sender、delivery marker。

### D. payload 构造按代码读

```text
PushRuntime::build_payload
-> IMHeader push_header
   cmd_id = CMD_PUSH_MESSAGE
   from_uid = context.sender_uid 或 service id
   to_uid = receiver_uid
-> PushRequest body
   type = PUSH_MESSAGE
   content = content
   related_message_id = msg_id
   ext = {"sender_uid","conversation_type","conversation_id"}
-> ProtobufCodec::encode(push_header, push_req, encoded)
```

这说明 Web 客户端收到 push 后，需要通过 `ext` 判断是 direct 还是 group，以及属于哪个会话。

### E. 本地 Push 按代码读

```text
gateway/push/PushService
-> runtime_(this, this, this)
```

`PushService` 同时实现三个运行时依赖：

```text
get_sessions
-> conn_mgr_->get_user_sessions(receiver_uid)

send_payload
-> ws_server_->get_session(session_id)
-> session->send(payload)

mark_delivered
-> msg_client_->mark_delivered(msg_id, delivered_time)
```

这就是本地模式为什么能直接发送 WebSocket：它运行在 Gateway 进程内，拿得到 ConnectionManager 和 WebSocketServer。

### F. 远程 Push 按代码读

Gateway 调远程 Push：

```text
RemotePushNotifier::notify_user
-> im.push.NotifyUserRequest
-> receiver_uid/msg_id/content/sender_uid/conversation_type/conversation_id
-> stub->NotifyUser
```

Push server 接到后：

```text
PushGrpcService::NotifyUser
-> PushRuntime::notify_user
```

远程 Push 要发 WebSocket 时回调 Gateway：

```text
GatewayPushDeliveryService::ListUserSessions
GatewayPushDeliveryService::SendSessionPayload
GatewayPushDeliveryService::MarkMessageDelivered
```

代码意义：

```text
远程 Push 做投递决策，Gateway 做实际 session 操作。
```

### G. FanoutPolicy 按代码读

```text
AllSessionsFanoutPolicy::select_sessions
-> 返回所有 session_id

PlatformFilterFanoutPolicy::select_sessions
-> 只返回 platform 在 allowed_platforms 中的 session

NewestSessionFanoutPolicy::select_sessions
-> 找 connect_time 最新的 session
```

面试落点：

```text
多端策略不是写死在 PushRuntime 中，而是通过策略类扩展。
```

### H. 面试追问如何指回代码

| 追问 | 指向代码 |
| --- | --- |
| Push 为什么 best-effort | `PushRuntime::notify_user` 中无 session/发送失败不抛给消息链路 |
| delivered 何时标记 | `success_count > 0 -> mark_delivered` |
| payload 里有什么 | `PushRuntime::build_payload` |
| 本地 Push 怎么发 WS | `PushService::send_payload` |
| 远程 Push 怎么碰 Gateway session | `GatewayPushDeliveryService` |
| 多端策略在哪 | `fanout_policy.cpp` |
