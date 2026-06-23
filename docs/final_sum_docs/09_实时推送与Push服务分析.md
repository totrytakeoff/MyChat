# 实时推送与 Push 服务分析

日期：2026-06-23

## 1. 文档目标

这份文档分析 MyChat 的在线实时推送设计：

- Push 为什么从消息服务中抽象出来。
- 本地 Push 和远程 Push 的区别。
- Gateway 为什么仍然持有 WebSocket session。
- PushRuntime、FanoutPolicy、Gateway callback 如何协作。
- Push 的 best-effort 语义和 delivered 标记边界。

一句话概括：

```text
Push 服务负责在线投递编排，但不拥有 WebSocket 连接；
Gateway 持有连接，Push 通过本地适配或远程 Gateway callback 完成 session 查询和 payload 发送。
```

## 2. Push 触发点

单聊：

```text
MessagePacketDispatcher 保存消息
-> 返回 MessagePacketResponse.push_event
-> Gateway message_command_handlers 或 MessageWsHandler
-> PushNotifier::notify_user
```

群聊：

```text
GroupPacketDispatcher 保存群消息
-> 计算除发送者外的成员 push_events
-> 返回 GroupPacketResponse.push_events
-> Gateway group_command_handlers
-> PushNotifier::notify_user(each receiver)
```

关键边界：

- Gateway 不计算单聊/群聊业务字段。
- Gateway 不计算群成员 fanout。
- service packet dispatcher 返回明确的 push event 元数据。
- Gateway 只转交 push event。

## 3. 模块职责

| 模块 | 职责 |
| --- | --- |
| MessagePacketDispatcher | 单聊持久化后返回 `MessagePushEvent` |
| GroupPacketDispatcher | 群消息持久化后返回 `GroupPushEvent` 列表 |
| Gateway command handler | 消费 push_event/push_events，调用 PushNotifier |
| PushNotifier | 推送调用抽象，本地/远程实现同一接口 |
| PushRuntime | 查询 session、应用 fanout policy、构造 payload、发送、标记 delivered |
| FanoutPolicy | 选择要投递的 session 子集 |
| ConnectionManager | Gateway 内部维护 uid -> sessions |
| WebSocketServer | Gateway 内部按 session_id 发送 payload |
| GatewayPushDeliveryService | 远程 Push 回调 Gateway 的 gRPC 服务 |

## 4. 本地 Push 模式

```text
Gateway command handler / MessageWsHandler
-> PushNotifier
-> PushRuntime
-> ConnectionManager::get_user_sessions
-> FanoutPolicy::select_sessions
-> build CMD_PUSH_MESSAGE + im.push.PushRequest
-> WebSocketServer / session->send
-> mark_delivered
```

本地模式适合单机 MVP 和快速调试。

## 5. 远程 Push 模式

远程 Push 是独立进程：

```text
Gateway
-> RemotePushNotifier
-> push_server PushService.NotifyUser
-> PushRuntime
-> GatewayPushDeliveryService.ListUserSessions
-> GatewayPushDeliveryService.SendSessionPayload
-> GatewayPushDeliveryService.MarkMessageDelivered
```

为什么需要 callback：

```text
WebSocket session 是 Gateway 进程内对象。
远程 push_server 不能直接访问 Gateway 内存里的 session map。
所以 remote push 必须通过 Gateway callback 回到 Gateway 完成实际投递。
```

## 6. FanoutPolicy

当前策略：

| 策略 | 语义 |
| --- | --- |
| `AllSessionsFanoutPolicy` | 推送给用户所有在线 session |
| `PlatformFilterFanoutPolicy` | 只推送给指定平台 |
| `NewestSessionFanoutPolicy` | 只推送给最新连接 session |

默认策略是全 session 推送，适合当前多窗口验证。

## 7. delivered 语义

当前 delivered 标记含义：

```text
至少一个 session 成功接受 push payload，或离线拉取后补标。
```

它不等于：

- 所有设备都收到。
- 用户已读。
- 端到端强确认。

生产级还需要：

- 客户端 ACK。
- 设备级投递日志。
- read receipt。
- 重试队列。

## 8. best-effort 取舍

Push 当前是 best-effort：

- 没有在线 session 时不报致命错误。
- session 发送失败不回滚消息。
- 远程 Push RPC 失败不影响已持久化消息。
- Push 成功只提升实时性。

可靠性兜底来自：

```text
先持久化 -> 再 Push -> Push 失败 -> offline/history 补偿
```

## 9. 源码导读

建议按顺序读：

1. `gateway/command_handlers/message_command_handlers.cpp`
   - `handle_send_message_http`
2. `gateway/ws/message_ws_handler.cpp`
   - `MessageWsHandler::handle_send`
3. `gateway/command_handlers/group_command_handlers.cpp`
   - `notify_group_push_events`
4. `services/push/push_runtime.cpp`
   - `PushRuntime::notify_user`
5. `services/push/fanout_policy.cpp`
6. `gateway/push/push_service.cpp`
7. `gateway/push/remote_push_notifier.cpp`
8. `gateway/push/gateway_push_delivery_service.cpp`

## 10. 面试追问

### Q: 为什么 Push 不直接放在 MessageService？

因为 MessageService 负责消息数据，Push 负责在线投递。把 Push 抽出来后，单聊和群聊可以复用同一套 fanout 和 session 投递逻辑。

### Q: 远程 Push 为什么要回调 Gateway？

因为连接归 Gateway 所有，远程 Push 服务没有 Gateway 进程内的 WebSocket session 对象。callback 是为了在保持 Push 独立服务的同时，不破坏连接所有权。

### Q: Push 失败会丢消息吗？

不会。发送成功的语义是消息已持久化，Push 失败只影响实时性，用户可以通过历史或离线拉取补偿。
