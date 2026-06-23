# MyChat 面试复盘与验证计划

日期：2026-06-23

## 1. 目标

本阶段把 MyChat 当作一个完整可演示的 C++ 分布式 IM 后端 MVP 来复盘，而不是继续无限扩功能。

复盘目标：

- 讲清楚当前系统已经实现了什么。
- 把每个用户操作对应到 Gateway、服务、存储和 Push 链路。
- 明确哪些是 MVP 已完成能力，哪些是生产级扩展。
- 准备面试中可能被追问的技术点。
- 为后续多机器分布式验证和容量测试提供路径。

## 2. 当前权威架构

当前 Gateway 主链路是统一 packet 架构：

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

复盘时不要再把旧的 HTTP controller / client facade 当作当前主架构。旧方案只能作为“为什么要重构”的历史背景。

## 3. 当前 MVP 能力

- Gateway HTTP 和 WebSocket 接入。
- 注册、登录、token 鉴权、个人资料查询和更新。
- 用户搜索。
- 好友申请、处理、好友列表。
- 单聊消息发送、历史、离线拉取、WebSocket ack、在线 push。
- 群创建、搜索、资料、加入、退出、成员列表、群消息、群历史、群 push。
- PushRuntime、FanoutPolicy、本地 PushNotifier、远程 Push gRPC、Gateway callback。
- Redis token/session 状态。
- PostgreSQL/ODB 持久化。
- `common/proto` 统一 proto/gRPC 生成链路。
- `clients/web` 中文 IM 风格验证客户端。
- 基础压测报告和重构前后对比。

## 4. 复盘原则

每个功能都按这个模板讲：

```text
用户动作
-> Gateway HTTP/WS contract
-> Gateway 统一 packet 链路
-> local dispatcher 或 remote ForwardPacket
-> service packet dispatcher
-> service/repository/runtime
-> Redis/PostgreSQL/WebSocket side effect
```

复盘时区分三类事项：

- MVP 已完成：当前可运行、可验证、可讲。
- 工程增强：压测、监控、部署、CI、多机验证。
- 产品扩展：富媒体、撤回、群管理、对象存储、黑名单。

## 5. 推荐阅读路径

### 5.1 项目总览

阅读：

- `README.md`
- `docs/final_sum_docs/01_项目整体架构总览.md`
- `docs/project/architecture/mvp_architecture.md`
- `docs/project/roadmap/current_progress.md`

重点：

- Gateway 为什么是统一入口。
- 统一 packet boundary 如何替代旧 controller/facade。
- local 和 remote 为什么都保持同一套 packet 契约。
- Web 客户端为什么只访问 Gateway。

面试追问：

- 为什么不让客户端直接访问每个微服务？
- Gateway 的职责边界是什么？
- Gateway 如何避免变成业务聚合层？
- local 与 remote 模式的差异在哪里？

### 5.2 Gateway 统一入口与路由

阅读：

- `docs/final_sum_docs/04_Gateway统一接入与服务编排分析.md`
- `docs/project/architecture/gateway_protobuf_packet_boundary_baseline.md`
- `gateway/gateway_server/gateway_server.cpp`
- `gateway/gateway_command_handler_registry.*`
- `gateway/gateway_runtime_registry.*`
- `common/network/message_parser.*`
- `common/network/message_processor.*`

重点：

- HTTP/WS 如何进入统一消息模型。
- cmd_id 如何路由到服务。
- Gateway 为什么只读 envelope 和 metadata。
- push_event 如何从服务结果交给 Push。

面试追问：

- Gateway 不解析业务 payload，怎么做鉴权？
- 新增一个业务服务需要改哪些地方？
- RuntimeRegistry 如何选择 local 或 remote endpoint？

### 5.3 注册登录与鉴权

阅读：

- `docs/final_sum_docs/05_注册登录与鉴权链路分析.md`
- `services/user/user_packet_dispatcher.cpp`
- `services/user/user_service.*`
- `services/user/user_repository.*`
- `services/user/user_grpc_service.*`
- `common/proto/user.proto`
- `common/redis/*`

验证：

- 注册两个用户。
- 登录并获取 token。
- 刷新页面确认 token 状态。
- 编辑昵称、签名、性别、头像。

面试追问：

- token 为什么由 Gateway 处理？
- Gateway 如何把 actor uid 传给服务？
- Redis 和 PostgreSQL 在鉴权链路中如何分工？
- 头像当前为何用文本字段，生产如何改？

### 5.4 单聊消息

阅读：

- `docs/final_sum_docs/06_单聊消息链路分析.md`
- `services/message/message_packet_dispatcher.cpp`
- `services/message/message_service.*`
- `services/message/message_repository.*`
- `services/message/message_grpc_service.*`
- `gateway/ws/message_ws_handler.*`
- `common/proto/message.proto`

验证：

- 两个账号在线发送单聊。
- 接收方不刷新页面收到 push。
- 断开接收方后发送离线消息。
- 重连后通过历史或离线拉取看到消息。

面试追问：

- 为什么先持久化再推送？
- ack、delivered、read 区别是什么？
- 如何防止伪造 sender_uid？
- Push 失败后如何补偿？

### 5.5 好友关系

阅读：

- `docs/final_sum_docs/07_好友关系链路分析.md`
- `services/friend/friend_packet_dispatcher.cpp`
- `services/friend/friend_service.*`
- `services/friend/friend_repository.*`
- `services/friend/friend_grpc_service.*`
- `common/proto/friend.proto`

验证：

- 搜索用户。
- 发送好友申请。
- 另一个账号查看 pending 并接受。
- 双方好友列表刷新。

面试追问：

- 好友申请和好友关系如何建模？
- 如何避免重复申请和反向申请？
- 为什么搜索用户不放在 Friend Service？

### 5.6 群聊

阅读：

- `docs/final_sum_docs/08_群聊链路分析.md`
- `services/group/group_packet_dispatcher.cpp`
- `services/group/group_service.*`
- `services/group/group_repository.*`
- `services/group/group_grpc_service.*`
- `common/proto/group.proto`

验证：

- 创建群。
- 另一个账号搜索群并加入。
- 查看群资料和成员。
- 发送群消息并观察在线 push。
- 从群资料页退出群。

面试追问：

- 群成员权限在哪里校验？
- 群消息 fanout 如何避免发送者重复收到？
- 大群 fanout 如何优化？
- 当前群设置 UI 哪些还不是后端持久能力？

### 5.7 Push 与实时投递

阅读：

- `docs/final_sum_docs/09_实时推送与Push服务分析.md`
- `services/push/push_runtime.*`
- `services/push/fanout_policy.*`
- `services/push/push_grpc_service.*`
- `gateway/push/push_service.*`
- `gateway/push/remote_push_notifier.*`
- `gateway/push/gateway_push_delivery_service.*`
- `common/proto/push.proto`

验证：

- 单聊在线 push。
- 群聊多成员 push。
- 切换不同 fanout policy 的单元测试。
- 远程 Push 模式下 callback 路径。

面试追问：

- Push 为什么独立成服务？
- 远程 Push 为什么要回调 Gateway？
- 多 Gateway 如何找到用户连接？
- best-effort 投递的边界是什么？

### 5.8 gRPC 与 proto 生成

阅读：

- `docs/final_sum_docs/10_gRPC服务边界与远程调用分析.md`
- `docs/project/runtime/proto_generation.md`
- `common/proto/*.proto`
- root `CMakeLists.txt`

重点：

- `common/proto` 是唯一 proto 来源。
- `generate_proto` 是聚合生成目标。
- Gateway remote endpoint 使用 ForwardPacket，而不是直接理解每个业务 RPC。
- 服务内部仍可保留业务 RPC 或 dispatcher，但 Gateway 主链路以 packet boundary 为准。

面试追问：

- 为什么用 protobuf/gRPC？
- 如何避免生成文件漂移？
- proto 如何做兼容演进？

### 5.9 存储与缓存

阅读：

- `docs/final_sum_docs/11_数据持久化与缓存设计分析.md`
- `db/migrations/001_core_schema.sql`
- `scripts/db/migrate_postgres.sh`
- `services/odb/*`
- `common/database/*`
- `common/redis/*`

面试追问：

- Redis 和 PostgreSQL 如何分工？
- ODB 相比手写 SQL 的优缺点是什么？
- 为什么 migration 不放在服务启动里自动执行？
- 如何做生产 schema 演进？

### 5.10 Web 验证客户端

阅读：

- `clients/web/README.md`
- `docs/modules/web_client.md`
- `docs/project/architecture/web_validation_client_plan.md`
- `clients/web/src`

验证：

- 双账号注册登录。
- 好友申请与接受。
- 单聊实时 push。
- 群搜索、加入和群聊。
- 个人资料和群资料页。
- token 过期后回到登录页。

面试追问：

- 为什么后端项目要写 Web 客户端？
- Web 如何验证 WebSocket protobuf 二进制消息？
- 后续迁移 Electron/Qt 怎么复用交互模型？

## 6. 多机器验证规划

建议在单机验证稳定后再做：

```text
Machine A: Gateway HTTP/WebSocket + Web client
Machine B: User/Message/Friend/Group gRPC services
Machine C: Push server
Shared: Redis/PostgreSQL
```

检查项：

- 所有 `*.remote_endpoint` 必须是可从 Gateway 访问的地址。
- `push.gateway_delivery_endpoint` 必须能从 Push server 访问 Gateway。
- 防火墙开放 Gateway HTTP、Gateway WS、服务 gRPC、Push gRPC、Gateway callback。
- 日志先确认 endpoint、token、cmd_id，再判断业务失败。

## 7. 简历表述建议

```text
基于 C++20 实现分布式 IM 后端，设计 Gateway 统一 HTTP/WebSocket 接入与 packet
转发链路，后端拆分 User、Message、Friend、Group、Push 服务，支持本地 dispatcher
和远程 gRPC ForwardPacket 两种部署模式；实现注册登录、好友、单聊、群聊、在线推送、
离线补偿、Redis token/session 状态、PostgreSQL/ODB 持久化，并通过 React Web 客户端
和压测报告完成端到端验证。
```

## 8. 下一批文档输出

- 按 `docs/final_sum_docs` 顺序继续细读并标注源码入口。
- 把多机器部署验证结果补入 `15_多机器分布式部署验证方案.md`。
- 把后续 todo 收敛到 `docs/TODO`，不要散落在历史文档中。
- 压测新增数据统一落到 `docs/benchmark` 并回链到最终总结文档。
