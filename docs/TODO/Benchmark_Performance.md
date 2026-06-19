# Benchmark 与性能优化 TODO

本 TODO 来自 2026-06-19 压测分析，详细背景见：

- `docs/benchmark/benchmark_report_20260619/benchmark_report_20260619.md`
- `docs/benchmark/benchmark_report_20260619/performance_issue_analysis_and_optimization_plan.md`

## P0: 入口层与部署参数

- [ ] 调整 gateway 启动脚本，确保进程 soft `ulimit -n` 至少为 65535。
- [ ] 修复 HTTP 10002 listen backlog 过小问题，目标 backlog 1024 或 4096。
- [ ] 统一记录 WSS/HTTP 监听参数，压测前自动采集 `ss -ltnp` 与 `/proc/<pid>/limits`。
- [ ] 将 benchmark 脚本中的公网 IP、PVE 密码、私有机器信息迁移到 env/config，避免写死。
- [ ] 清理 benchmark 构建产物，避免提交二进制文件。

## P1: Gateway 消息调度模型

- [x] 替换 `MessageProcessor::process_message` 中每消息 `std::async(std::launch::async)` 模型，统一投递到全局 `ThreadPool`。
- [x] 移除 Gateway WSS 回调中的每消息 `std::thread(...).detach()` 等待 future 模型，WS 入口直接投递一个业务任务并同步执行处理核心。
- [x] 认证失败/认证超时的延迟关闭不再创建 detached 线程，统一走线程池调度。
- [x] 引入 WSS `max_ws_inflight_messages` 初步流控，覆盖排队中和执行中的消息。
- [x] 超过 WSS inflight 上限时返回明确 overload 响应，避免无限排队到秒级 RTT。
- [x] 为 WSS 处理路径增加 inflight 数量、线程池线程数、线程池排队/执行任务数统计。
- [ ] 继续抽象独立业务执行器与真正 bounded queue，使 Gateway 不直接依赖全局线程池。
- [ ] 增加 WSS 单条消息处理耗时统计和分位数指标。

本轮验证：

```bash
cmake --build build/remote-push-odb --target gateway_server -j2
docker compose up -d redis postgres
ctest --test-dir build/remote-push-odb -R GatewayMessageWsTest --output-on-failure
```

结果：`gateway_server` 构建通过，`GatewayMessageWsTest` 通过。

本轮修复记录：

- `docs/history/devlog/phase19_gateway_ws_backpressure.md`

## P1: 消息持久化路径

- [ ] 优化 `MessageService::send_text_message`，插入后直接基于持久化对象构造返回，避免立即 `find_by_id`。
- [ ] 优化 `MessageRepository::mark_delivered`，使用按 `msg_id` 直接 update 的方式，避免 load + update。
- [ ] 检查并补充消息表索引：`sender_uid`、`receiver_uid`、`create_time`、`status`。
- [ ] 增加 message repository 层轻量 benchmark 或集成测试，验证插入与 delivered 更新耗时。

## P2: Push 链路解耦

- [ ] 将发送 ack 与 push fanout 解耦：消息持久化成功后优先 ack。
- [ ] 引入 push worker 队列，异步处理在线推送。
- [ ] 将 delivered 标记从发送主路径移到 push worker。
- [ ] 记录 push 成功、push 失败、delivered 更新失败等可观测指标。

## P2: 压测体系完善

- [ ] WSS 指标拆分：连接耗时、ack RTT、push latency、业务持久化成功数。
- [ ] HTTP 指标拆分：连接失败、HTTP 非 2xx、业务错误、请求超时。
- [ ] 增加 `200u/400ms`、`200u/333ms`、`200u/250ms` 独立冷却复测。
- [ ] HTTP ramp 拆分 `/health`、`/auth/info`、真实业务 API 三类场景。
- [ ] 压测报告自动记录 commit hash、部署模式、配置文件、机器规格、冷却时间。

## 当前建议执行顺序

1. 先处理 P0，避免继续被入口配置误导。
2. 再处理 P1 中的线程模型和 DB 写入路径，因为它们是当前 WSS 过载的核心风险。
3. P2 作为后续性能专项，不阻塞当前项目总结与面试准备。
