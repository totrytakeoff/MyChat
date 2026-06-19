# TODO 索引

用于记录后续可优化方向以及待解决问题。

## 当前重点

- [Benchmark_Performance.md](Benchmark_Performance.md)：压测暴露的问题、已修复项和后续性能优化计划。
- [Gateway.md](Gateway.md)：Gateway 结构和路由层面的长期优化项。

## 当前状态

2026-06-19 已完成 Gateway WSS 调度模型第一阶段修复：

- `std::async(std::launch::async)` 每消息任务模型已替换为全局 `ThreadPool` 投递。
- WSS 回调中的每消息 detached 等待线程已移除。
- WSS 增加 `max_ws_inflight_messages` 初步流控。
- 认证失败和认证超时的延迟关闭已改为线程池任务。

详细记录见：

- `docs/history/devlog/phase19_gateway_ws_backpressure.md`
- `docs/benchmark/benchmark_report_20260619/performance_issue_analysis_and_optimization_plan.md`
