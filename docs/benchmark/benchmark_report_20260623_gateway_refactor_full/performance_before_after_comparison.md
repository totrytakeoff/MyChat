# 2026-06-23 Gateway 重构前后性能对比分析

## 1. 对比目的

本报告用于补充 `2026-06-23 Gateway 重构后完整压测报告`，重点回答一个问题：

> Gateway 从旧 controller/client 链路重构为统一 packet 转发链路后，性能和稳定性是否退化，是否能作为当前 MVP 的稳定基准。

结论先行：

- HTTP ramp 提升最明显：重构前大量请求卡到 60s timeout，重构后同口径压测失败率为 `0.00%`，p95 降到 `3.75ms`。
- WSS 建连能力保持稳定：`conn-10/50/100/200` 均为全成功。
- WSS 消息吞吐在重构后拿到了有效基准：`200 users / 250ms` 场景达到 `714.4 msg/s`，RTT p95 `35.91ms`，错误为 0。
- 重构后 Gateway 压测结束无 session 残留、无线程池任务堆积。

## 2. 数据来源与口径说明

主要对比来源：

- 重构前：`docs/benchmark/benchmark_report_20260622_lzr_full/benchmark_report_20260622_lzr_full.md`
- 重构后：`docs/benchmark/benchmark_report_20260623_gateway_refactor_full/benchmark_report_20260623_gateway_refactor_full.md`

辅助参考来源：

- 历史异常完整压测：`docs/benchmark/benchmark_report_20260621_full/benchmark_report_20260621_full.md`

口径限制：

- HTTP ramp 可以直接对比。两轮均使用 `lzr-host -> SUT`，HTTP 端口均为 `10002`，压测阶段均包含 `10 -> 50 -> 100 -> 200 -> 500 VUs`。
- WSS 建连可以直接对比。两轮均覆盖 `conn-10/50/100/200`。
- WSS 消息吞吐不能用 2026-06-22 作为直接对比基准，因为该报告明确记录 WSS 消息日志无有效样本，Gateway stats 中 `processed websocket message count:0`。
- 2026-06-21 的 WSS 消息数据只能作为历史异常状态参考，不能作为严格同环境 A/B 数据。

## 3. HTTP Ramp 对比

| 指标 | 重构前 2026-06-22 | 重构后 2026-06-23 | 变化 |
| --- | ---: | ---: | ---: |
| HTTP 请求数 | 2595 | 46296 | 约 `17.8x` |
| 平均 RPS | 14.44/s | 306.86/s | 约 `21.3x` |
| 失败率 | 56.03% | 0.00% | 高并发 timeout 消失 |
| 平均延迟 | 8.23s | 2.86ms | 秒级降到毫秒级 |
| p90 | 59.99s | 3.55ms | 60s 长尾消失 |
| p95 | 59.99s | 3.75ms | 60s 长尾消失 |
| max | 59.99s | 16.84ms | timeout 长尾消失 |

### 解读

重构前 HTTP 的核心问题不是所有接口都慢，而是高并发阶段大量请求长时间拿不到响应，最终触发 k6 的 60s timeout。

证据：

- 2026-06-22 的 `http_req_duration med=45.88ms`，说明至少一半请求仍能较快返回。
- 但总体 p90/p95/max 均为 `59.99s`，说明另一批请求被卡到超时。
- `/health` 是轻量健康检查，仍出现 `health_duration p95=59998ms`，因此问题更像 HTTP 接入层调度、连接处理、keep-alive、线程队列或入口状态，而不是业务 SQL/Redis 慢查询。

重构后：

- `46296` 个请求全部通过。
- HTTP ramp 失败率 `0.00%`。
- p95 为 `3.75ms`，max 为 `16.84ms`。

这说明当前 HTTP 接入层已经从“高并发大量 timeout”回到“可稳定压测”的状态。

## 4. WSS 建连对比

| 场景 | 重构前 2026-06-22 | 重构后 2026-06-23 |
| --- | ---: | ---: |
| `conn-10` | 10/10 | 10/10 |
| `conn-50` | 50/50 | 50/50 |
| `conn-100` | 100/100 | 100/100 |
| `conn-200` | 200/200 | 200/200 |
| Gateway accept fail | 0 | 0 |
| 压测结束 session 残留 | 0 | 0 |
| 线程池任务残留 | 0 | 0 |

重构后 `conn-200` 连接延迟：

| 指标 | 数值 |
| --- | ---: |
| 成功连接 | 200/200 |
| Connect p95 | 14ms |
| Connect p99 | 25ms |

### 解读

WSS 建连能力在重构前已经通过 `lzr-host -> SUT` 路径验证为稳定 200 连接；重构后保持稳定，没有出现连接生命周期退化。

这点很重要：本轮重构改动了 Gateway 主链路、handler 注册和 service 转发边界，但没有破坏 WSS accept、session add、关闭清理和线程池状态。

## 5. WSS 消息吞吐基准

2026-06-22 的 WSS 消息吞吐样本无效，因此本节只记录重构后有效基准。

| 场景 | 用户数 | 间隔 | Messages sent | Messages recv | Throughput | RTT p50 | RTT p95 | RTT p99 | Errors |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `ws-200u-1s` | 200 | 1s | 12890 | 25780 | 186.8 msg/s | 6.28ms | 8.84ms | 12.35ms | 0 |
| `ws-200u-500ms` | 200 | 500ms | 13880 | 27760 | 355.9 msg/s | 9.89ms | 15.55ms | 20.44ms | 0 |
| `ws-200u-250ms` | 200 | 250ms | 27860 | 55707 | 714.4 msg/s | 20.36ms | 35.91ms | 45.98ms | 0 |

### 历史异常参考

2026-06-21 旧压测中，`ws-200u-250ms` 参考数据为：

| 指标 | 2026-06-21 历史异常 | 2026-06-23 重构后 |
| --- | ---: | ---: |
| 成功连接 | 36/200 | 200/200 |
| 失败连接 | 164 | 0 |
| 发送吞吐 | 97.1/s | 714.4 msg/s |
| RTT p95 | 65.45ms | 35.91ms |
| 压测后连接残留 | `98 ESTAB + 21 CLOSE-WAIT` | `current_sessions=0` |

该对比不是严格 A/B，但能说明当前系统已经脱离早期“连接建不上、关不干净、压测样本污染”的状态。

## 6. Gateway 最终状态对比

重构前 2026-06-22 WSS 阶段后：

```text
ws.accept_ok: 4077
ws.accept_fail: 0
ws.current_sessions: 0
ws.inflight_messages: 0
thread_pool.queued_or_running_tasks: 0
```

重构后 2026-06-23 完整压测后：

```text
ws.accept_ok: 1570
ws.accept_fail: 0
ws.current_sessions: 0
processed websocket message count: 68556
parse.decode failed count: 0
parse.routing failed count: 1
ws.inflight_messages: 0
http.total_requests: 46723
http.status_2xx: 46521
http.status_4xx: 200
http.status_5xx: 1
thread_pool.queued_or_running_tasks: 0
```

重构后关键点：

- WSS 有真实消息处理样本：`processed websocket message count=68556`。
- 解码失败为 0，说明统一 packet 链路没有出现大面积协议解析问题。
- 压测结束后 `ws.current_sessions=0`、`ws.inflight_messages=0`、`thread_pool.queued_or_running_tasks=0`。
- HTTP ramp 自身失败率为 0；最终 stats 中少量 4xx/5xx 来自准备/探测阶段，不代表 ramp 失败。

## 7. 架构重构与性能结果的关系

本轮性能改善不能简单归因于“少了一层 controller 所以更快”。更准确的说法是：

1. Gateway 主入口统一后，请求处理链路更可控。
   - 重构前 HTTP controller/client、local/remote 分支和业务入口分散，排查高并发 timeout 时难以确认请求究竟卡在哪一层。
   - 重构后 HTTP/WSS 都进入 `MessageParser -> MessageProcessor -> GatewayCommandHandlerRegistry -> packet dispatcher / ForwardPacket`，压测问题更容易被归因到入口、路由、服务转发或下游 service。

2. Gateway 不再解析业务 payload，热路径职责更清晰。
   - Gateway 只负责连接管理、鉴权、路由、packet 转发和 push 转交。
   - user/message/friend/group 的字段解析、业务校验、响应构造回收到各 service packet dispatcher。

3. 删除 controller/client 混杂路径后，HTTP 入口和 WSS 入口更一致。
   - 这降低了维护成本，也减少了“某个 HTTP route 走旧路径、某个 WS cmd 走新路径”的不确定性。

4. 性能稳定性改善还依赖此前的连接生命周期、keep-alive、线程池、HTTP 接入层修复。
   - 因此面试或总结中应把它表述为“多轮压测定位 + Gateway 入口收口 + 生命周期修复 + HTTP 接入修复后的综合结果”，不要夸大为单次重构带来的纯性能提升。

## 8. 当前可用于项目总结的结论

可以写入最终项目总结和面试材料的结论：

- MyChat Gateway 已完成从 controller/client 混杂入口到统一 packet 转发架构的重构。
- 重构后在云端 `lzr-host -> SUT` 压测环境下：
  - WSS `200` 并发建连全成功，connect p95 `14ms`。
  - WSS `200 users / 250ms` 场景达到 `714.4 msg/s`，RTT p95 `35.91ms`，错误为 0。
  - HTTP ramp 到 `500 VUs` 时完成 `46296` 请求，平均 `306.86 req/s`，失败率 `0.00%`，p95 `3.75ms`。
- 相比重构前，HTTP ramp 从 `56.03%` 失败率、p95 `59.99s` timeout，恢复到 0 失败和毫秒级延迟。
- 压测结束后 Gateway 无 WSS session 残留、无 inflight 消息残留、无线程池任务堆积。

## 9. 后续建议

短期不建议继续为了追求更高数字扩大 Gateway 重构范围。当前更重要的是：

1. 修复 `gateway_server` CLI/config 优先级，避免压测端口被配置覆盖。
2. 统一压测环境 Redis 密码策略，避免远端临时改配置。
3. 将 stats 输出改为 JSON，方便 benchmark 自动采集。
4. 补充混合真实业务压测：好友申请、加群、群聊、历史拉取。
5. 补充 ack RTT、push latency、DB 持久化耗时拆分指标。

当前基准已经足够支撑项目进入最终总结与面试准备阶段。
