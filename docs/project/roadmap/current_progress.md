# MyChat 当前进度

日期：2026-06-23

## 1. 当前结论

MyChat 当前已经进入“完整 MVP + 面试总结 + 稳定性验证”阶段。

项目不再按“还缺一个生产 IM 功能就继续扩”的方式推进。当前优先级是：

- 保持现有注册登录、好友、单聊、群聊、Push、Web 验证客户端稳定可演示。
- 用统一 packet 架构重新整理 Gateway 和各服务文档。
- 用压测数据说明当前能力和瓶颈。
- 为后续多机器部署、面试讲解和简历项目展示做准备。

## 2. 当前权威架构

Gateway 主链路：

```text
HTTP / WebSocket
-> MessageParser
-> UnifiedMessage
-> MessageProcessor
-> GatewayCommandHandlerRegistry
-> GatewayRuntimeRegistry
-> local service packet dispatcher 或 remote ForwardPacket gRPC
-> service 解析业务 payload 并执行业务逻辑
```

当前 Gateway 只负责：

- HTTP/WS 协议入口。
- token 鉴权和 actor uid 边界。
- WebSocket session 管理。
- cmd_id 路由。
- packet 转发。
- push_event handoff。
- 响应回写和错误码转换。

Gateway 不负责：

- 解析业务 payload 字段。
- 维护业务 DTO mapper。
- 执行好友、群成员、消息状态等业务规则。
- 作为旧式 HTTP controller/client facade 聚合层。

业务解析位置：

- `services/user/user_packet_dispatcher.cpp`
- `services/message/message_packet_dispatcher.cpp`
- `services/friend/friend_packet_dispatcher.cpp`
- `services/group/group_packet_dispatcher.cpp`

## 3. 已完成能力

### 3.1 Gateway

- HTTP 和 WebSocket 统一接入。
- MessageParser / UnifiedMessage / MessageProcessor 统一消息链路。
- GatewayCommandHandlerRegistry 按 cmd_id 注册处理入口。
- GatewayRuntimeRegistry 选择 local dispatcher 或 remote ForwardPacket endpoint。
- WebSocket session 管理。
- 鉴权失败、过期 token、连接关闭等基础错误处理。
- WS 心跳与超时关闭逻辑。
- 全局线程池与 WS inflight 限流，避免每消息创建线程。

### 3.2 User

- 注册。
- 登录。
- token 生成由 Gateway 负责。
- 个人资料查询和更新。
- 用户搜索。
- PostgreSQL/ODB 持久化。
- gRPC 服务边界和独立 `user_server`。
- packet dispatcher + ForwardPacket 远程边界。

### 3.3 Message

- 单聊消息发送。
- 历史查询。
- 离线消息拉取。
- delivered/read 状态标记。
- WebSocket send/ack。
- 先持久化再 push。
- PostgreSQL/ODB 持久化。
- gRPC 服务边界和独立 `message_server`。
- packet dispatcher + ForwardPacket 远程边界。

### 3.4 Friend

- 用户搜索后发起好友申请。
- pending 申请查询。
- 接受/拒绝申请。
- 好友列表。
- 重复申请、自加好友、反向申请等校验。
- PostgreSQL/ODB 持久化。
- gRPC 服务边界和独立 `friend_server`。
- packet dispatcher + ForwardPacket 远程边界。

### 3.5 Group

- 建群。
- 搜索群。
- 群资料。
- 加入/退出群。
- 群成员列表。
- 群消息发送和历史。
- 群消息 push_events 返回，由 Gateway handoff 给 Push。
- PostgreSQL/ODB 持久化。
- gRPC 服务边界和独立 `group_server`。
- packet dispatcher + ForwardPacket 远程边界。

### 3.6 Push

- PushNotifier 边界。
- PushRuntime。
- FanoutPolicy：
  - AllSessionsFanoutPolicy。
  - PlatformFilterFanoutPolicy。
  - NewestSessionFanoutPolicy。
- 本地 PushService 适配 Gateway session。
- 远程 PushService.NotifyUser gRPC。
- GatewayPushDeliveryService callback：
  - ListUserSessions。
  - SendSessionPayload。
  - MarkMessageDelivered。
- Push best-effort 语义。

### 3.7 Web 验证客户端

- `clients/web` 中文 IM 风格验证客户端。
- 注册/登录。
- 个人资料查看和编辑。
- 好友搜索、申请、处理、好友列表。
- 单聊实时消息。
- 群搜索、加入、群资料、群成员、群聊。
- 未读红点。
- Enter / Ctrl+Enter 发送配置。
- Debug drawer 保留调试能力，但不暴露在主流程中。

## 4. 存储与基础设施

- Redis：
  - token/session 等临时状态。
  - hiredis RAII 连接池。
  - 连接池并发、重连和 token 生命周期测试。
- PostgreSQL：
  - 用户、消息、好友、群组持久数据。
  - ODB 2.5.0 映射。
  - `db/migrations/001_core_schema.sql` 核心 schema。
  - `scripts/db/migrate_postgres.sh` 显式迁移。
- Proto/gRPC：
  - `common/proto` 是唯一 proto 来源。
  - `generate_proto` 是聚合生成目标。
  - 各服务提供业务 RPC 和/或 ForwardPacket 边界，Gateway 主链路以 packet boundary 为准。

## 5. 当前验证状态

已验证：

- 本地开发栈可运行。
- Web 双账号流程可验证核心 IM 功能。
- HTTP API 基础测试已跑通过。
- Gateway 统一 packet 重构后完成完整压测。
- 重构前后性能报告已落入 `docs/benchmark`。

最近压测基准：

- WSS `conn-200`：`200/200` 成功，connect p95 `14ms`，p99 `25ms`。
- WSS `200 users / 250ms`：`714.4 msg/s`，RTT p95 `35.91ms`，p99 `45.98ms`，errors `0`。
- HTTP ramp：`46296` requests，`306.86 req/s`，failure `0.00%`，p95 `3.75ms`。

对比旧压测：

- 2026-06-22 旧 HTTP ramp：`2595` requests，`14.44 req/s`，failure `56.03%`，p95 `59.99s`。
- 2026-06-23 重构后 HTTP ramp：失败率归零，延迟降到毫秒级。

结论：

当前项目已经具备可演示、可面试讲解、可继续多机验证的 MVP 稳定性。压测数据只能作为当前 MVP 基准，不能夸大为生产级容量。

## 6. 当前文档状态

已重写或正在作为权威文档维护：

- `docs/final_sum_docs/`
- `docs/modules/`
- `docs/project/architecture/`
- `docs/project/roadmap/`
- `docs/benchmark/`
- `docs/TODO/`

旧阶段日志、旧 controller/client facade 设计和历史上下文，应放在：

- `docs/history/`
- `docs/project/architecture/gateway_unified_message_refactor_baseline.md`
- `docs/project/architecture/gateway_handler_boundary_refactor_requirements.md`
- `docs/project/architecture/gateway_protobuf_packet_boundary_baseline.md`

如果文档中把旧 `UserHttpController`、`MessageClient`、`Local*Client`、`Remote*Client` 当作当前主链路，应视为过期内容。

## 7. 后续优先级

### P0：面试总结与文档一致性

- 完成 `docs/final_sum_docs` 的逐篇源码引用增强。
- 保证所有当前文档统一描述 packet boundary。
- 把 TODO 与生产级扩展分离，避免干扰当前 MVP 结论。

### P1：多机器分布式验证

- Gateway 与服务分机器部署。
- Push server 与 Gateway callback 跨机器验证。
- 检查远程 endpoint、防火墙、回调地址、Redis/PostgreSQL 地址。

### P2：性能与稳定性

- 继续压测 HTTP、WS、Push fanout。
- 引入分段观测：数据库写入、packet dispatch、PushRuntime、WS send。
- 评估是否引入异步 Push 队列。

### P3：生产级功能扩展

- 多 Gateway 连接路由。
- 消息序列号和更强顺序语义。
- 富媒体和对象存储。
- 消息撤回、编辑、搜索。
- 黑名单、隐私规则。
- 群管理：管理员、踢人、禁言、审核、群主转让。
- 监控、告警、部署编排和生产 TLS/secret 管理。

## 8. 风险和约束

- ODB 2.5.0 runtime 需要本地构建，启用 ODB 前要确认工具链。
- ODB 重型测试仍建议串行跑，避免共享测试数据互相污染。
- GitHub hosted CI 曾因工程化成本影响开发节奏，当前以本地脚本为主，最终收尾阶段再恢复 CI。
- Web 客户端用于验证后端能力，不代表最终产品级 UI。
- 当前单 Gateway 架构不等于生产级横向扩展方案，多 Gateway 是后续重点。
