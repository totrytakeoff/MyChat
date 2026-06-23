# 2026-06-23 Gateway 重构后完整压测报告

## 1. 测试目的

本轮压测用于验证 `94399b2 Refactor gateway unified packet flow` 后的 Gateway 主链路稳定性。

重点确认：

- 统一 `MessageParser -> MessageProcessor -> GatewayCommandHandlerRegistry -> packet dispatcher / ForwardPacket` 后，HTTP/WSS 主链路是否仍稳定。
- 删除旧 controller/client 后，注册、登录、HTTP info、WSS 单聊消息是否能承受完整压测流程。
- 对比 2026-06-22 旧压测中 HTTP ramp 大量 timeout 的问题是否仍然存在。

原始日志归档：

```text
docs/benchmark/benchmark_report_20260623_gateway_refactor_full/raw_logs/
```

## 2. 测试环境

代码版本：

```text
94399b2 Refactor gateway unified packet flow
```

SUT：

- Host/IP: `122.51.70.33`
- SSH: `ASD-Host`
- Gateway WSS: `10001`
- Gateway HTTP: `10002`
- Gateway binary: `/opt/mychat/bin/gateway_server`
- Config: `/opt/mychat/config/benchmark.json`

发压端：

- SSH: `lzr-host`
- Hostname: `VM-4-10-debian`
- `bench_ws`: `/tmp/benchmark/bench_ws`
- `k6`: `/tmp/benchmark/k6`
- tokens: `/tmp/benchmark/users.json`
- users: `200`

运行命令：

```bash
BENCH_CLOUD_HOST=122.51.70.33 \
BENCH_CLOUD_SSH=ASD-Host \
BENCH_CLOUD_WSS_PORT=10001 \
BENCH_CLOUD_HTTP_PORT=10002 \
BENCH_PVE_HOST=lzr-host \
BENCH_PVE_USER=myself \
BENCH_PVE_PASS='' \
BENCH_PVE_TOOL_DIR=/tmp/benchmark \
python3 test/benchmark/run_all.py
```

本轮准备阶段修正：

- SUT Redis 实例未配置密码，`benchmark.json` 中 `redis.password` 已在远端运行配置中改为空字符串。
- Gateway 配置端口改回压测端使用的 `10001/10002`。
- `bench_ws` 在 `lzr-host` Debian 12 本机重新编译，避免本地构建产物依赖 `GLIBC_2.38`。

## 3. 总体结论

本轮结果比 2026-06-22 旧压测明显更稳定。

- WSS 纯建连 `10/50/100/200` 全成功，`Connects FAIL = 0`。
- WSS 消息压测从低频到 `200 users / 250ms` 全成功，业务错误为 `0`。
- 最重 WSS 场景达到 `714.4 msg/s`，RTT `p95=35.91ms`，`p99=45.98ms`。
- Gateway 最终 stats：`ws.accept_ok=1570`，`ws.accept_fail=0`，`ws.current_sessions=0`。
- HTTP ramp：`46296` requests，`306.86 req/s`，`http_req_failed=0.00%`。
- HTTP 延迟：`avg=2.86ms`，`p95=3.75ms`，`max=16.84ms`。
- 压测结束后 Gateway 仍运行，线程池无堆积，WSS session 无残留。

这轮可以作为“重构后稳定性基准数据”使用。

## 4. WSS 连接压力

| 场景 | 连接数 | 成功 | 失败 | Connect p95 | Connect p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `conn-10` | 10 | 10 | 0 | 15ms | 15ms |
| `conn-50` | 50 | 50 | 0 | 13ms | 30ms |
| `conn-100` | 100 | 100 | 0 | 13ms | 37ms |
| `conn-200` | 200 | 200 | 0 | 14ms | 25ms |

结论：

- 200 并发建连全部成功。
- 和旧报告中 PVE 路径出现的 `125` 建连阈值不同，本轮 `lzr-host -> SUT` 路径未出现应用层建连瓶颈。
- TLS/WS 建连延迟稳定在十几毫秒级，远低于上一轮公网异常路径的长尾。

## 5. WSS 消息吞吐

| 场景 | 用户数 | 间隔 | Messages sent | Messages recv | Throughput | RTT p50 | RTT p95 | RTT p99 | Errors |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `ws-10u-10s` | 10 | 10s | 59 | 118 | 1.0 msg/s | 6.65ms | 8.47ms | 10.30ms | 0 |
| `ws-50u-10s` | 50 | 10s | 299 | 598 | 4.8 msg/s | 6.46ms | 7.42ms | 8.82ms | 0 |
| `ws-100u-5s` | 100 | 5s | 1199 | 2398 | 18.7 msg/s | 6.46ms | 7.84ms | 10.06ms | 0 |
| `ws-50u-5s` | 50 | 5s | 599 | 1198 | 9.7 msg/s | 6.41ms | 7.72ms | 9.42ms | 0 |
| `ws-100u-2s` | 100 | 2s | 3077 | 6154 | 48.1 msg/s | 6.38ms | 8.14ms | 11.25ms | 0 |
| `ws-200u-5s` | 200 | 5s | 2498 | 4996 | 36.2 msg/s | 6.32ms | 8.02ms | 9.81ms | 0 |
| `ws-100u-1s` | 100 | 1s | 6195 | 12390 | 96.8 msg/s | 6.53ms | 8.39ms | 10.76ms | 0 |
| `ws-200u-1s` | 200 | 1s | 12890 | 25780 | 186.8 msg/s | 6.28ms | 8.84ms | 12.35ms | 0 |
| `ws-200u-500ms` | 200 | 500ms | 13880 | 27760 | 355.9 msg/s | 9.89ms | 15.55ms | 20.44ms | 0 |
| `ws-200u-250ms` | 200 | 250ms | 27860 | 55707 | 714.4 msg/s | 20.36ms | 35.91ms | 45.98ms | 0 |

解读：

- 低频和中频场景 RTT p95 基本在 `8ms` 左右。
- 压力场景 `200 users / 250ms` 下 RTT p95 上升到 `35.91ms`，但仍处于可接受范围。
- `Messages recv` 基本约等于 `Messages sent * 2`，符合 ack + push 的压测口径。
- 最重场景出现了可观察的延迟上升，但没有错误、连接失败或线程池持续堆积。

## 6. HTTP Ramp

HTTP ramp 阶段：

```text
10 -> 50 -> 100 -> 200 -> 500 VUs
```

关键结果：

| 指标 | 数值 |
| --- | ---: |
| `http_reqs` | 46296 |
| 平均 RPS | 306.86/s |
| `http_req_failed` | 0.00%, 0/46296 |
| `http_req_duration avg` | 2.86ms |
| `http_req_duration med` | 2.89ms |
| `http_req_duration p90` | 3.55ms |
| `http_req_duration p95` | 3.75ms |
| `http_req_duration max` | 16.84ms |
| `health_duration avg` | 2.35ms |
| `info_duration avg` | 3.38ms |

和 2026-06-22 `benchmark_report_20260622_lzr_full` 对比：

| 指标 | 2026-06-22 | 2026-06-23 重构后 |
| --- | ---: | ---: |
| HTTP 请求数 | 2595 | 46296 |
| 平均 RPS | 14.44/s | 306.86/s |
| 失败率 | 56.03% | 0.00% |
| p95 | 59.99s | 3.75ms |
| max | 59.99s | 16.84ms |

结论：

- 上一轮 HTTP 高并发下大量请求卡到 60s timeout 的问题，本轮没有复现。
- 当前 HTTP 接入层在该压测口径下表现稳定。
- 这轮改动中 HTTP worker/queue/keep-alive 配置和 Gateway 统一入口收口后，至少没有引入新的 HTTP 性能退化。

## 7. Gateway 最终状态

最终 Gateway stats 摘要：

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

说明：

- `ws.current_sessions=0` 表示 WSS 压测结束后连接清理正常。
- `thread_pool.queued_or_running_tasks=0` 表示压测结束后没有业务线程池任务残留。
- `http.status_4xx=200` 主要来自压测准备/重复账号/认证类前置流量，不代表 HTTP ramp 失败；k6 ramp 自身失败率为 `0/46296`。
- `http.status_5xx=1` 出现在准备/探测阶段，HTTP ramp 没有失败。

## 8. 发现的问题

本轮压测准备阶段发现两个工程问题：

1. `gateway_server` 命令行 `-w/-H` 会被 `benchmark.json` 中 `gateway.websocket_port/http_port` 覆盖。
   - 表现：命令行传 `10001/10002`，实际监听 `8101/8102`。
   - 处理：本轮远端运行配置已改回 `10001/10002`。
   - 后续建议：明确 CLI 与 config 的优先级，通常命令行应覆盖配置。

2. SUT Redis 是无密码实例，但本地 `config/benchmark.json` 写了 `mychat-dev-pass`。
   - 表现：新版 Gateway 启动时报 `Redis AUTH failed`。
   - 处理：本轮远端运行配置将 `redis.password` 改为空字符串。
   - 后续建议：压测配置和远端实际依赖要统一，避免部署时临时修改。

## 9. 后续建议

短期建议：

- 把本轮结果作为重构后基准，不再继续扩大 Gateway 重构范围。
- 修正 `gateway/main.cpp` 中 CLI/config 优先级，避免后续部署误判端口。
- 统一 `config/benchmark.json` 与 SUT Redis 实际密码策略。
- 将本轮报告纳入最终项目总结，用作“压测与稳定性验证”证据。

中期建议：

- 继续补充 HTTP/WSS 混合真实业务场景压测，例如好友申请、群聊、历史拉取。
- 将 stats 输出改成 JSON，便于 benchmark 脚本自动解析和报告生成。
- 对 `ws-200u-250ms` 的延迟上升做持续观察，但当前不是阻塞问题。

