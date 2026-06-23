# Push Service 模块说明

## 当前职责

Push Service 负责在线消息投递的服务边界和 fanout 逻辑：

- 接收 NotifyUser 请求。
- 根据用户在线 session 做 fanout 选择。
- 构造 `CMD_PUSH_MESSAGE` + `im.push.PushRequest`。
- 调用 Gateway 持有的 WebSocket session 完成实际发送。
- 成功发送至少一个 session 后标记消息 delivered。
- 支持本地 PushNotifier 和远程 gRPC PushNotifier。

## 对外入口

### Push gRPC

定义在 `common/proto/push.proto`：

```text
im.push.PushService.NotifyUser
```

### Gateway callback gRPC

因为 WebSocket 连接由 Gateway 持有，远程 Push 服务需要回调 Gateway：

```text
im.push.GatewayPushDeliveryService.ListUserSessions
im.push.GatewayPushDeliveryService.SendSessionPayload
im.push.GatewayPushDeliveryService.MarkMessageDelivered
```

## 核心链路

### 本地模式

```text
Gateway Message/Group packet handler
-> PushNotifier
-> gateway/push/PushService
-> services/push/PushRuntime
-> ConnectionManager 查询 session
-> WebSocketServer 发送 payload
-> Gateway delivery marker 标记 delivered
```

### 远程模式

```text
Gateway entrypoint
-> RemotePushNotifier
-> push_server PushGrpcService
-> PushRuntime
-> GatewayPushDeliveryService callback
-> Gateway 查询 session / 发送 WebSocket payload / 标记 delivered
```

## FanoutPolicy

当前已存在的 fanout 策略：

- `AllSessionsFanoutPolicy`：推送到所有在线 session。
- `PlatformFilterFanoutPolicy`：按平台过滤 session。
- `NewestSessionFanoutPolicy`：只推送到最新 session。

默认策略保持全 session 推送，便于多端验证。

## 关键设计

- Push 是 best-effort，不保证在线投递一定成功。
- 消息持久化不依赖 Push 成功。
- 远程 Push 不直接持有 WebSocket session，而是通过 Gateway callback 完成投递。
- delivered 标记发生在至少一个 session 成功发送之后。

## 面试可讲点

- 为什么 Push 从 Message 中拆出来。
- 为什么远程 Push 需要 Gateway callback。
- WebSocket session 由 Gateway 持有带来的扩展问题。
- fanout policy 如何支持多端策略。
- Push 失败时为什么不回滚消息。
- 多 Gateway 实例下如何定位用户连接。

## 后续扩展

- 多 Gateway 场景下的连接位置注册表。
- 服务发现和负载均衡。
- 离线 push 队列和重试诊断。
- 按用户设置进行免打扰/平台过滤。
- Push 投递指标和链路追踪。
