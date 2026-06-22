# 2026-06-22 lzr-host 完整压测报告

## 1. 测试目的

本轮把压测端切到 `lzr-host`，对同一台 SUT `122.51.70.33` 执行一轮完整压测，目标是同时确认三件事：

1. `lzr-host -> SUT` 公网路径是否仍会复现 PVE 发压时的 `125` WSS 建连阈值。
2. Gateway 在连续 WSS 建连、保活、关闭后是否仍能保持干净状态。
3. HTTP ramp 在高并发下的真实表现，以及下一步瓶颈定位方向。

原始日志归档：

- `docs/benchmark/benchmark_report_20260622_lzr_full/raw_logs/`

远端原始目录：

```text
lzr-host:/tmp/mychat_lzr_full_20260622_124023
```

## 2. 测试环境

SUT:

- Host/IP: `122.51.70.33`
- Gateway WSS: `10001`
- Gateway HTTP: `10002`
- Gateway stats: `GET /api/v1/stats`

Gateway 启动参数：

```bash
/opt/mychat/bin/gateway_server \
  -c /opt/mychat/config/benchmark.json \
  -w 10001 \
  -H 10002 \
  -l info \
  --max-open-files 65535
```

发压端：

- SSH alias: `lzr-host`
- Hostname: `VM-4-10-debian`
- OS: Debian 12
- `ulimit -n`: `65535`
- `bench_ws`: `/tmp/bench_ws`
- tokens: `/tmp/users.json`
- users: `200`

依赖补齐：

```bash
sudo apt-get install -y --no-install-recommends libprotobuf32 libssl3 ca-certificates
```

## 3. WSS 建连结果

### 3.1 纯建连矩阵

| 场景 | 目标用户数 | 持续时间 | 连接速率 | 结果 |
| --- | ---: | ---: | ---: | --- |
| `conn-10` | 10 | 30s | 默认极低消息频率 | 10/10 |
| `conn-50` | 50 | 30s | 默认极低消息频率 | 50/50 |
| `conn-100` | 100 | 30s | 默认极低消息频率 | 100/100 |
| `conn-200` | 200 | 30s | 默认极低消息频率 | 200/200 |

结论：

- `lzr-host -> SUT` 公网路径没有复现 PVE 压测时 `200 users` 只成功 `125` 的问题。
- 同一 SUT 在 `lzr-host` 公网路径下可以稳定建立 200 条 WSS 连接。
- 这与前面的 SUT loopback、`lzr-host` public 补测结论一致：PVE 的 `125` 阈值更像是 PVE 出口、源 IP、NAT、路由路径或云侧针对该源的策略问题，不是 Gateway 应用层固定上限。

### 3.2 Gateway 侧累计观测

完整 WSS 阶段结束后，Gateway stats 摘要：

```text
ws.accept_ok: 4077
ws.accept_fail: 0
ws.active_handshakes: 0
ws.current_sessions: 0
ws.ssl_handshake.count: 4077
ws.ssl_handshake.avg_ms: 21.3642
ws.ssl_handshake.max_ms: 10126
ws.upgrade_read.count: 4052
ws.upgrade_read.avg_ms: 8.91807
ws.upgrade_read.max_ms: 341
ws.accept_handshake.count: 4052
ws.accept_handshake.avg_ms: 0.145114
ws.accept_handshake.max_ms: 13
ws.session_add.count: 4045
ws.session_add.avg_ms: 0.172559
ws.session_add.max_ms: 11
ws.inflight_messages: 0
thread_pool.queued_or_running_tasks: 0
```

解读：

- Gateway 没有 accept 失败。
- 压测结束后 `current_sessions=0`，没有 WSS session 残留。
- 线程池无排队任务，说明 WSS 生命周期和清理路径本轮表现稳定。
- `ssl_handshake.max_ms=10126` 说明仍存在少量 TLS 建连长尾，需要后续在更大样本下继续观察，但当前不影响 200 连接成功率。

## 4. WSS 消息吞吐口径说明

本轮 WSS 消息吞吐数据不作为有效结论。

原因：

- 归档到本地的各 WSS 场景 `.txt` 只有阶段头，没有完整 `bench_ws` 结果体。
- Gateway stats 中 `processed websocket message count:0`。
- 终端观测也显示消息发送统计没有形成可审计的有效样本。

因此本轮只采纳 WSS 建连、关闭清理和 Gateway stats 作为有效证据；不使用本轮数据讨论 WSS 业务消息吞吐、ack RTT 或 push latency。

后续必须先修复 benchmark 工具与归档脚本：

1. 每个 WSS 场景必须把 `bench_ws stdout/stderr` 完整写入对应日志。
2. 每个消息场景必须校验 `messages_sent > 0`，否则直接标记该场景无效。
3. 报告生成脚本必须拒绝用无消息样本计算吞吐和 RTT。

## 5. HTTP Ramp 结果

HTTP 脚本：

- `test/benchmark/http_benchmark.js`
- 每轮迭代请求：
  - `GET /api/v1/health`
  - `GET /api/v1/auth/info`，带 `Bearer token`
- VU 阶段：
  - 10s -> 10 VU
  - 30s -> 50 VU
  - 30s -> 100 VU
  - 30s -> 200 VU
  - 30s -> 500 VU
  - 20s -> 0 VU

关键结果：

| 指标 | 数值 |
| --- | ---: |
| `http_reqs` | 2595 |
| 平均 RPS | 14.44/s |
| `iterations` | 1201 |
| 完成迭代速率 | 6.68/s |
| `http_req_failed` | 56.03%, 1454/2595 |
| `http_req_duration avg` | 8.23s |
| `http_req_duration med` | 45.88ms |
| `http_req_duration p90` | 59.99s |
| `http_req_duration p95` | 59.99s |
| `http_req_duration max` | 59.99s |
| interrupted iterations | 420 |

接口维度：

| 指标 | avg | med | p90 | p95 | max |
| --- | ---: | ---: | ---: | ---: | ---: |
| `health_duration` | 12580ms | 4.85ms | 59998ms | 59998ms | 59998ms |
| `info_duration` | 3217ms | 46.50ms | 50.51ms | 58641ms | 59998ms |

阈值结果：

```text
http_req_duration p(95)<500    failed, actual p95=59.99s
http_req_duration p(99)<2000   failed, actual p99=59.99s
request_failed rate<0.01       failed, actual rate=56.03%
```

## 6. HTTP 结果解读

这不是“接口普遍很慢”，而是“高并发下大量请求被卡到超时”。

证据：

- `http_req_duration med=45.88ms`，说明至少一半请求可以较快返回。
- successful response 的 `p95=5.44ms`，说明成功返回的 `/health` 类请求非常快。
- 总体 `p90/p95=59.99s`，说明另一批请求长时间没有拿到响应，最终触发 k6 request timeout。
- `/health` 是纯内存健康检查，仍出现 `p90/p95` 接近 60s，问题优先怀疑 HTTP server 接入层调度、连接处理、线程模型、accept/backlog/连接队列或压测路径，而不是业务 SQL、Redis 或 token 校验本身。

当前 Gateway stats 只覆盖 WSS 和通用线程池，缺少 HTTP 侧关键指标，所以还不能直接断言具体代码点。

下一步需要补的观测：

1. HTTP server 线程数、工作队列、活跃请求数。
2. `/health` 与 `/auth/info` 分接口请求数、成功数、失败数、耗时分位数。
3. HTTP accept/keep-alive/连接关闭状态。
4. SUT `ss -s`、`ss -tan state established '( sport = :10002 )'`、`netstat -s` 前后增量。
5. `httplib::Server` 是否在当前编译/运行配置下使用了过小的默认线程池或连接调度队列。

## 7. 本轮结论

### 已确认

1. `lzr-host -> SUT` 公网 WSS 建连稳定，200 用户场景全成功。
2. PVE 上的 `125` 建连阈值不是 Gateway 应用层固定上限，也不是 SUT 对所有单源公网 IP 的普遍限制。
3. Gateway WSS 生命周期修复有效，本轮结束后无 session 残留，无线程池堆积。
4. HTTP ramp 在 100+ VU 后开始暴露严重 timeout，500 VU 阶段失败率达到 `56.03%`。

### 未确认

1. WSS 业务消息吞吐本轮无有效样本，不能使用。
2. HTTP timeout 的具体代码瓶颈尚未定位，当前只能确定它发生在 HTTP 高并发接入/调度链路，不能直接归咎于业务数据库或 Redis。
3. PVE 路径的 `125` 阈值还需要 tcpdump/netstat 增量来给最终证据。

## 8. 下一步建议

优先级建议：

1. 先修 benchmark 工具链：保证 WSS 消息压测日志完整归档，并对 `messages_sent=0` 做失败保护。
2. 对 HTTP ramp 做拆分复测：
   - 只压 `/health`
   - 只压 `/auth/info`
   - 分别跑 50/100/200/500 VU
   - 本机 loopback 和 `lzr-host` 公网各跑一组
3. 给 Gateway HTTP 入口增加轻量 stats，至少覆盖请求数、状态码、inflight、耗时分位数。
4. 检查并明确 `cpp-httplib` 的线程模型配置，必要时设置固定线程池/任务队列上限。
5. 暂时不要继续盲改消息业务链路；当前最有价值的是把 HTTP/WSS 压测观测补齐，形成可解释、可复现的瓶颈证据链。
