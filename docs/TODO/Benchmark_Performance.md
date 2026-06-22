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

## P0: WSS 连接生命周期与关闭清理

背景:

- 2026-06-21 完整压测报告: `docs/benchmark/benchmark_report_20260621_full/benchmark_report_20260621_full.md`
- 压测结束后 gateway 仍存活，`ulimit=65535`、WSS/HTTP backlog 均为 `4096`，但 WSS 残留 `98 ESTAB + 21 CLOSE-WAIT`。
- 已建立连接的 WSS 消息 RTT 基本为几十毫秒，当前首要问题是连接建不上、关不干净，而不是消息处理链路本身慢。

修复项:

- [x] 重构 `WebSocketSession::close()` 为幂等关闭流程，保证多次调用只清理一次。
- [x] 所有 SSL/WebSocket/read/write/handshake 错误都触发统一 session 清理，不再只 log 后返回。
- [x] graceful close 失败时仍强制 shutdown/close 底层 socket，并执行 `WebSocketServer::remove_session()`。
- [x] `WebSocketServer::remove_session()` 触发 disconnect handler，ConnectionManager 和 Redis 在线映射继续沿现有断连回调清理。
- [x] 增加 Beast WebSocket handshake timeout、idle timeout 与 keep-alive ping，避免半开连接长期占用 fd 和 session。
- [x] 修复 `WebSocketServer::stop()` 持锁关闭 session 的潜在死锁风险。
- [x] 注册 `CMD_HEARTBEAT` 业务心跳 handler，返回 protobuf `BaseResponse`，并校验 token/device。
- [ ] 将认证失败/认证超时/踢号的延迟关闭从全局业务线程池 `sleep_for` 迁移到 Asio timer 或 session 内部 timer。

验收标准:

- [x] 跑完 `conn-50/100/200` 后，gateway 进程仍存活。
- [x] 复测确认服务端 WSS `CLOSE-WAIT=0`。
- [x] 无活跃客户端时，服务端 WSS `ESTAB` 在 idle timeout 后回落到 0。
- [ ] `bench_ws` 不再出现退出挂起。
- [ ] `conn-100/conn-200` 在干净状态下重新统计成功率和 connect p95，避免被上一轮残留连接污染。

本轮验证：

```bash
cmake --build build/remote-push-odb --target gateway_server -j2
cmake --build build/remote-push-odb --target gateway_server test_gateway_message_ws -j2
docker compose up -d redis postgres
ctest --test-dir build/remote-push-odb -R GatewayMessageWsTest --output-on-failure
./build/remote-push-odb/test/gateway_message/test_gateway_message_ws --gtest_filter='GatewayMessageWsTest.Heartbeat*'
```

结果：`gateway_server` 构建通过，`GatewayMessageWsTest` 通过，`GatewayMessageWsTest.Heartbeat*` 2 个业务心跳用例通过。

本轮修复记录：

- `docs/history/devlog/phase19_gateway_ws_backpressure.md`
- `docs/history/devlog/phase20_gateway_ws_lifecycle_heartbeat.md`
- `docs/benchmark/benchmark_report_20260621_ws_lifecycle_retest/benchmark_report_20260621_ws_lifecycle_retest.md`

复测结论:

- `conn-10/50/100` 全部成功，`conn-200` 为 `125/200`。
- 压测结束并等待 idle timeout 后，WSS 端口只剩 `LISTEN`，无 `CLOSE-WAIT`，无 `ESTAB` 残留。
- 当前 P0 生命周期问题视为通过；剩余问题转为 WSS 建连性能专项。

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

- [x] WSS 建连指标拆分：resolve、TCP connect、TLS handshake、WS handshake、失败阶段分类。
- [x] Gateway WSS handshake 链路增加服务端观测：accept、TLS、HTTP upgrade、WS accept、session add。
- [x] 增加 `GET /api/v1/stats` 只读观测入口，便于远端压测抓取 Gateway 内部状态。
- [ ] 修复 WSS 消息压测归档：每个场景必须完整保存 `bench_ws stdout/stderr`，并在 `messages_sent=0` 时直接判定场景无效。
- [ ] WSS 消息指标拆分：ack RTT、push latency、业务持久化成功数。
- [ ] HTTP 指标拆分：连接失败、HTTP 非 2xx、业务错误、请求超时。
- [ ] 增加 `200u/400ms`、`200u/333ms`、`200u/250ms` 独立冷却复测。
- [ ] HTTP ramp 拆分 `/health`、`/auth/info`、真实业务 API 三类场景。
- [ ] HTTP ramp 增加本机 loopback 与公网双路径对照，先单独压 `/health` 再单独压 `/auth/info`。
- [ ] Gateway HTTP 入口增加请求数、状态码、inflight、分接口耗时分位数等轻量观测。
- [ ] 压测报告自动记录 commit hash、部署模式、配置文件、机器规格、冷却时间。

## P1: WSS 公网建连专项

背景:

- 2026-06-21 WSS 建连观测专项报告: `docs/benchmark/benchmark_report_20260621_ws_observe/benchmark_report_20260621_ws_observe.md`

当前结论:

- PVE 公网发压 `100 users @ 20/s` 全成功。
- PVE 公网发压 `150/200 users` 稳定只有 `125` 个连接成功，失败全部为 `connect_timeout`。
- 发压机 `ulimit -n` 拉高到 `65535` 后，`150 users` 仍为 `125/150`，基本排除发压进程 fd 上限。
- SUT 本机 loopback `150 users @ 20/s` 全成功，connect avg `2.80ms`，说明 Gateway 进程和 WSS 应用层不是当前 125 阈值的主因。
- 2026-06-22 SUT 本机 loopback 追加 `200 users @ 20/40/80/200/s` 矩阵，4 轮共 `800` 次建连全部成功，进一步确认 Gateway 本机建连能力不是瓶颈。
- 2026-06-22 `lzr-host` 公网发压追加 `200 users @ 20/40/80/200/s` 矩阵，4 轮共 `800` 次建连全部成功，说明 SUT 云服务器并不存在所有单源公网 IP 都卡 `125` 的普遍入口限制。
- Gateway stats 显示进入应用层的连接 TLS/HTTP upgrade/WS accept/session_add 都能正常完成，瓶颈主要落在公网 TCP 建连路径或入口侧网络策略。
- 2026-06-22 `lzr-host` 完整压测中 `conn-10/50/100/200` 全成功，继续证明该问题不是 Gateway 应用层固定上限，也不是 SUT 对所有单源公网 IP 的普遍限制。

待办:

- [ ] 压测前后分别采集 `netstat -s`，记录 TCP/SYN/reset/retransmit 增量，而不是累计值。
- [x] 使用第二台公网机器 `lzr-host` 作为发压端复测，确认 `lzr-host -> SUT` 公网路径 `200 users @ 200/s` 可全成功。
- [ ] 继续排查当前 PVE 到云服务器链路限制，重点关注 PVE 出口 NAT、路由器、防火墙、运营商路径或该源 IP 的云侧策略。
- [ ] SUT 使用 `tcpdump` 抓取 `tcp port 10001`，确认失败连接的 SYN/SYN-ACK/ACK 是否完整。
- [ ] 将 SUT `net.ipv4.tcp_max_syn_backlog` 从 `512` 调整到 `4096/8192` 后复测公网建连。
- [ ] 扩展 `--connect-rate 5/10/20/40` 矩阵，确认 125 阈值是否与突发连接速率或公网链路策略相关。
- [ ] 将 `bench_ws --timeout` 临时提高到 `30s`，观察超时连接是否只是延迟成功。
- [ ] benchmark 脚本支持自动抓取 `/api/v1/stats`、`ss`、`netstat -s`、`/proc/<pid>/limits` 并归档。

## 当前建议执行顺序

1. 先修压测工具链有效性：WSS 消息场景必须有完整日志，且 `messages_sent=0` 不能进入吞吐结论。
2. 专项处理 HTTP `/health` 与 `/auth/info` 高并发 timeout，先拆分接口和 loopback/public 路径，再补 Gateway HTTP 侧观测。
3. 保留 PVE 公网建连专项，但优先级下调；当前已有 SUT loopback 与 `lzr-host` public 双重证据说明 Gateway 不是 125 阈值主因。
4. 再回到消息链路压测：ack RTT、push latency、DB 持久化耗时。
5. 性能专项不阻塞当前项目总结与面试准备，但结论要写入最终面试材料：当前瓶颈定位方法和证据链比单纯 QPS 数字更重要。
