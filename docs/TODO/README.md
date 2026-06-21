# TODO 索引

用于记录后续可优化方向以及待解决问题。

## 当前重点

- [Benchmark_Performance.md](Benchmark_Performance.md)：压测暴露的问题、已修复项和后续性能优化计划。
- [Gateway.md](Gateway.md)：Gateway 结构和路由层面的长期优化项。

## 当前状态

2026-06-21 最新压测显示 Gateway crash 问题已收敛，随后已完成 WSS 连接生命周期第一轮修复：

- 完整压测结束后服务端残留 `98 ESTAB + 21 CLOSE-WAIT`。
- 已修复 `WebSocketSession` 错误路径、close 清理幂等性、强制 socket close、`WebSocketServer::stop()` 持锁关闭风险。
- 已补齐 Beast idle timeout/keepalive ping 与 `CMD_HEARTBEAT` 业务心跳 handler。
- 已完成远端复测：压测结束并等待 idle timeout 后，WSS 只剩 `LISTEN`，`CLOSE-WAIT=0`，`ESTAB=0`。
- 已补齐 `bench_ws` 建连阶段统计、Gateway WSS handshake 统计和 `/api/v1/stats` 观测入口。
- 当前剩余重点转为 WSS 公网建连专项：PVE 公网压测 `150/200 users` 稳定只有 `125` 个连接成功，但 SUT 本机 loopback `150 users` 全成功，优先排查公网 TCP 建连路径、云侧安全策略和 SUT TCP 参数。
- 详细条目见 `Benchmark_Performance.md` 和 `Gateway.md`。

最新报告：

- `docs/benchmark/benchmark_report_20260621_ws_observe/benchmark_report_20260621_ws_observe.md`

2026-06-19 已完成 Gateway WSS 调度模型第一阶段修复：

- `std::async(std::launch::async)` 每消息任务模型已替换为全局 `ThreadPool` 投递。
- WSS 回调中的每消息 detached 等待线程已移除。
- WSS 增加 `max_ws_inflight_messages` 初步流控。
- 认证失败和认证超时的延迟关闭已改为线程池任务。

详细记录见：

- `docs/history/devlog/phase19_gateway_ws_backpressure.md`
- `docs/benchmark/benchmark_report_20260619/performance_issue_analysis_and_optimization_plan.md`
