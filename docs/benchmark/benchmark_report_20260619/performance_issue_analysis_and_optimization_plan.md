# MyChat 2026-06-19 压测问题分析与优化建议

## 1. 背景

本轮压测目标是验证 MyChat 后端 MVP 在云端部署下的 WebSocket 实时消息链路与 HTTP API 入口的承载能力。有效日志主要来自：

- `/tmp/benchmark/logs/20260619_154205_*`
- `/tmp/benchmark/logs/20260619_154205_HTTP-Ramp-retry.txt`

早期 `20260619_151924_*` 多数输出只有 68 字节，属于无效运行结果，不作为性能判断依据。

## 2. 结论先行

当前性能问题不是单点算法慢，而是入口配置、并发调度模型、同步业务链路叠加造成的排队和过载。

主要判断：

1. WSS 消息链路在 `200 users / 500ms` 场景下仍稳定，RTT p95 约 72.76ms，0 业务错误。
2. WSS 在 `200 users / 250ms` 场景下明显过载，RTT 均值进入秒级，接收数低于发送数，并出现 200 个错误。
3. HTTP ramp 的主要问题不是业务接口慢，而是 HTTP 入口 accept/backlog 与 fd 配置不足导致连接阶段超时。
4. 当前压测数据可以证明“功能链路已打通并具备初步稳定区间”，但不能直接包装成“生产级高并发能力”。

## 3. 关键数据

### 3.1 WSS 稳定区间

| 场景 | 连接成功 | 连接失败 | 发送 | 接收 | 错误 | RTT avg | RTT p95 |
|---|---:|---:|---:|---:|---:|---:|---:|
| 100u / 1s | 100 | 1 | 5900 | 11800 | 0 | 57.27ms | 83.17ms |
| 200u / 1s | 200 | 1 | 11800 | 23600 | 0 | 50.08ms | 75.22ms |
| 200u / 500ms | 200 | 3 | 11765 | 23530 | 0 | 48.17ms | 72.76ms |

说明：

- `Messages recv` 在成功场景约为 `2x sent`，大概率包含 ack 与 push/echo，不能直接等同于业务消息投递量。
- 当前更稳妥的表述是：在 200 在线连接、约 230 msg/s 的端到端统计吞吐下，WSS RTT p95 可维持在 80ms 内。

### 3.2 WSS 过载点

| 场景 | 连接成功 | 发送 | 接收 | 错误 | RTT avg | RTT p95 |
|---|---:|---:|---:|---:|---:|---:|
| 200u / 250ms | 200 | 23102 | 13068 | 200 | 6261.78ms | 12102.23ms |

说明：

- 该场景已经不是“性能下降”，而是进入积压/失败状态。
- 过载边界大致位于 `200u / 500ms` 与 `200u / 250ms` 之间，需要补充 `200u / 333ms`、`200u / 400ms` 等独立复测。

### 3.3 建连长尾

| 用户数 | 成功 | 失败/超时记录 | 连接 avg | p50 | p95 |
|---:|---:|---:|---:|---:|---:|
| 10 | 10 | 0 | 369ms | 397ms | 601ms |
| 50 | 50 | 1 | 2023ms | 1444ms | 2563ms |
| 100 | 100 | 27 | 9666ms | 3343ms | 35585ms |
| 200 | 200 | 21 | 8683ms | 5866ms | 40660ms |

说明：

- 稳态消息 RTT 不差，但建连 p95 长尾很严重。
- 连接失败/超时记录可能包含 benchmark 端统计口径问题，但长尾本身真实存在。
- 需要进一步区分 TLS 握手、WebSocket upgrade、token 绑定、压测端连接超时等阶段。

### 3.4 HTTP ramp

HTTP retry 汇总：

| 指标 | 数值 |
|---|---:|
| 总请求 | 2683 |
| 吞吐 | 14.94 req/s |
| 失败率 | 22.99% |
| 总体 p95 | 59.95s |
| info p95 | 127.63ms |
| health p95 | 59.96s |

云端入口状态：

```text
HTTP 10002 listen backlog = 5
WSS  10001 listen backlog = 4096
gateway soft Max open files = 1024
```

说明：

- HTTP 的失败主要是 `dial: i/o timeout` 与 `request timeout`，优先怀疑入口连接队列与 fd 配置，而不是业务 handler 慢。
- `/auth/info` 的成功请求 p95 约 127ms，说明成功进入业务链路后的延迟不算离谱。

## 4. 问题定位

### 4.1 HTTP 入口配置过弱

代码位置：

- `gateway/gateway_server/gateway_server.cpp:1358`

当前 HTTP server 只执行 `bind_to_port("0.0.0.0", port)`，线上表现为 listen backlog 只有 5。高并发短连接 ramp 下，大量请求在连接阶段超时。

影响：

- HTTP ramp 被入口层打穿，不能代表真实业务接口上限。
- 500 VU 阶段的数据只能说明入口配置不合理，不能直接作为 HTTP 业务吞吐结论。

### 4.2 每条消息动态创建线程/异步任务

代码位置：

- `gateway/message_processor/message_processor.cpp:101`
- `gateway/gateway_server/gateway_server.cpp:1092`

修复前 WSS 每条消息：

1. 调用 `MessageProcessor::process_message`。
2. 内部使用 `std::async(std::launch::async)`。
3. Gateway 又额外创建一个 `std::thread(...).detach()` 等待 future 并发送响应。

问题：

- 高频消息场景下会产生大量线程创建、销毁、调度和上下文切换。
- 过载时没有统一队列长度、并发上限和拒绝策略，系统会从延迟恶化走向资源耗尽。

当前修复状态：

- `MessageProcessor::process_message` 已改为投递到全局 `ThreadPool`。
- Gateway WSS 回调已改为投递单个业务任务，并在任务内直接调用 `process_message_sync`，不再为每条消息创建等待 future 的 detached 线程。
- WSS 热路径已加入 `max_ws_inflight_messages` 初步流控，覆盖排队中和执行中的消息；超过上限时返回 `Gateway is busy, please retry later.`。
- 认证失败和认证超时的延迟关闭也已改为线程池任务，避免异常流量下继续创建 detached 线程。
- 通用 `MessageProcessor` 对 WebSocket 消息不再重复验签；`MessageWsHandler` 仍会校验 token 并从 token 推导真实 sender。

剩余优化：

- 当前流控是 inflight 上限，不是独立业务执行器的严格 bounded queue。
- 后续可拆出 Gateway 专用业务 executor，增加 queue size、drop/reject 计数、处理耗时直方图和分位数统计。

### 4.3 消息持久化路径 DB 操作偏重

代码位置：

- `services/message/message_service.cpp:41`
- `services/message/message_repository.cpp:20`
- `services/message/message_repository.cpp:31`

当前发送消息路径：

1. `persist(msg)` 插入消息。
2. 再 `find_by_id(msg.msg_id())` 读回消息。

问题：

- 一条消息至少产生 insert + select 两次 DB 操作。
- 在压测消息内容简单、业务只需要返回 msg_id 和基础字段时，二次读回收益不高。

### 4.4 push 与 delivered 标记同步耦合

代码位置：

- `services/push/push_runtime.cpp:93`
- `services/push/push_runtime.cpp:133`
- `services/message/message_repository.cpp:93`

当前 push 成功后同步执行 `mark_delivered`，其内部又是 load + update。

问题：

- ack、push、delivered 标记处在同一条发送处理路径上。
- 接收方在线推送越多，发送路径越容易受 socket 写队列、连接管理、DB 更新影响。

### 4.5 缺少过载保护与背压

已有发送队列上限：

- `common/network/websocket_session.cpp:54`

但整体仍缺少：

- 每连接 inflight 请求限制。
- 全局业务队列长度限制。
- 队列满时明确返回 overload。
- 压测指标中的 overload、timeout、queue_delay 拆分。

结果是高压下系统表现为 RTT 秒级、消息接收不完整、连接错误，而不是可控降级。

## 5. 优化建议

### P0: 立即修复入口与部署参数

目标：先排除配置型瓶颈，避免 HTTP 压测继续被入口误导。

建议：

1. gateway 启动脚本设置 `ulimit -n 65535`。
2. HTTP listen backlog 调整为 1024 或 4096。
3. WSS/HTTP 统一 socket 参数：`reuse_address`、必要时启用 `TCP_NODELAY`。
4. benchmark 文档中记录真实 SUT 配置、fd limit、listen backlog。

验收：

- `ss -ltnp` 中 HTTP 10002 backlog 不再是 5。
- `/proc/<pid>/limits` 中 gateway soft open files 不再是 1024。
- HTTP `/health` ramp 在 100/200 VU 下不再大量 dial timeout。

### P1: 替换每消息动态线程模型

目标：让 gateway 进入可控并发模型。

已完成：

1. 废弃 `std::async(std::launch::async)` 的每消息任务模型。
2. 使用全局固定线程池处理解析后的消息。
3. Gateway 不再每条正常 WSS 消息 `std::thread(...).detach()`。
4. 引入 `max_ws_inflight_messages` 初步流控，超过上限时快速返回 overload。

仍建议：

1. 抽象 Gateway 专用业务 executor。
2. 将 inflight cap 进一步演进为真正 bounded queue。
3. 补充处理耗时、拒绝数、队列长度的压测采集。

验收：

- `200u / 250ms` 下不再因线程资源出现 `std::system_error: Resource temporarily unavailable`。
- 压测时线程数稳定，不随消息数线性增长。

### P1: 精简消息写入与 delivered 更新

目标：降低单条消息的 DB 事务数。

建议：

1. `send_text_message` 插入后直接基于 `Message` 对象构造返回，不再立即 `find_by_id`。
2. `mark_delivered` 改成按 `msg_id` 直接 update，避免 load + update。
3. 对常用查询补充或确认索引：`sender_uid`、`receiver_uid`、`create_time`、`status`。

验收：

- 单条在线私聊从 insert + select + load + update 降低到 insert + update。
- WSS 稳定区间向 `200u / 333ms` 或 `200u / 250ms` 推进。

### P2: ack 与 push/delivered 解耦

目标：提高发送路径稳定性。

建议：

1. 持久化成功后优先返回 ack。
2. push fanout 进入异步队列。
3. delivered 标记由 push worker 异步执行。
4. push 失败不影响发送 ack，但记录可观测事件。

验收：

- 接收方在线/不在线不会显著影响发送 ack RTT。
- 高压下 ack RTT 不被 push/delivered 的 DB 更新放大。

### P2: 完善压测指标

目标：让数据可解释、可复现、可写进简历。

建议：

1. WSS 指标拆分为 connect latency、ack RTT、push latency、business persisted count。
2. HTTP 指标拆分为连接失败、非 2xx、业务错误、超时。
3. benchmark 脚本去除硬编码 IP、密码和机器私有信息。
4. 每轮压测记录启动参数、commit hash、部署模式、冷却时间。

## 6. 当前阶段建议

本轮数据已经足以支撑面试准备中的“性能压测与瓶颈定位”讨论，但不建议把性能优化全部展开成大重构。

建议当前只做 P0 和部分低风险 P1：

- 先修入口参数、fd limit、HTTP backlog。
- 修掉每消息动态线程这个明显架构隐患。
- 对 DB 写入做局部低风险优化。

暂缓：

- 完整异步 push worker。
- 完整背压体系。
- 大规模协程/异步 RPC 改造。
- 多机分布式性能扩展压测。

这些更适合作为面试中的后续优化路线，而不是当前打断项目总结与面试准备主线。
