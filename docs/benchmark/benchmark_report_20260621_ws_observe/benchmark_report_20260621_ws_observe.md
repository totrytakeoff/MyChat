# 2026-06-21 WSS 建连观测专项报告

## 1. 本轮目标

上一轮生命周期修复后，Gateway 已经不再出现压测后 `CLOSE-WAIT/ESTAB` 大量残留，但公网 WSS 建连仍在 `conn-200` 场景下稳定只有 `125/200` 成功。

本轮目标不是继续猜测业务链路，而是补齐观测后回答三个问题：

1. `bench_ws` 自身是否污染连接耗时统计。
2. 慢点到底在 DNS/TCP/TLS/WS upgrade/业务注册哪个阶段。
3. `125` 成功连接阈值来自 Gateway 本身，还是公网发压链路。

## 2. 本轮代码改动

### 2.1 `bench_ws` 压测工具修复

旧版 `bench_ws` 的启动模型有明显测量污染：

```cpp
for (...) {
    sessions[i]->start();
    sleep(stagger_us);
}
for (...) workers.emplace_back([&ioc] { ioc.run(); });
```

也就是说，主线程按 `connect-rate` sleep 时，`io_context` worker 还没启动，异步 resolve/connect/handshake 并没有真正推进。连接数越多，越会把本地排队时间算进 connect time。

本轮已修复为：

- 先创建 `io_context` work guard。
- 先启动 IO worker。
- 再按 `connect-rate` 将 session start 投递到 `io_context`。
- 连接统计新增阶段拆分：
  - `resolve`
  - `tcp_connect`
  - `ssl_handshake`
  - `ws_handshake`
  - total `connect_time`
- 失败统计新增阶段分类：
  - `resolve_fail`
  - `tcp_fail`
  - `ssl_fail`
  - `ws_fail`
  - `connect_timeout`
  - `post_connect_errors`

### 2.2 Gateway WSS 观测

Gateway WSS 入口新增轻量统计：

- `ws.accept_ok`
- `ws.accept_fail`
- `ws.active_handshakes`
- `ws.current_sessions`
- `ws.ssl_handshake.{count,avg_ms,max_ms}`
- `ws.upgrade_read.{count,avg_ms,max_ms}`
- `ws.accept_handshake.{count,avg_ms,max_ms}`
- `ws.session_add.{count,avg_ms,max_ms}`

并新增只读 HTTP 观测入口：

```text
GET /api/v1/stats
```

该入口早于 catch-all 路由注册，用于压测过程中直接抓取 Gateway 内部状态。

## 3. 部署与环境

SUT:

- Host: `122.51.70.33`
- SSH alias: `ASD-Host`
- Gateway:

```bash
/opt/mychat/bin/gateway_server \
  -c /opt/mychat/config/benchmark.json \
  -w 10001 \
  -H 10002 \
  -l info \
  --max-open-files 65535
```

新版 gateway hash:

```text
1291248247ab837764c1768410087b9c5caee825510e7e991d43209bd396bdb4  /opt/mychat/bin/gateway_server
```

发压机:

- Host: `192.168.1.185`
- Tool path: `/tmp/benchmark/bench_ws`
- Debian 12 兼容版 `bench_ws` hash:

```text
0986f6755264ed053ca4308c6b4ab2fd615be63a52b4fff32e4dc0d98057e379  /tmp/benchmark/bench_ws
```

SUT 关键运行参数:

```text
Max open files: 65535
WSS listen backlog: 4096
net.core.somaxconn: 4096
net.ipv4.tcp_max_syn_backlog: 512
```

## 4. 压测结果

原始日志归档：

- `docs/benchmark/benchmark_report_20260621_ws_observe/raw_logs/pve/`

### 4.1 PVE 公网发压

| 场景 | 成功 | 失败 | 失败类型 | connect p50 | connect p95 | TCP avg | TLS avg | WS avg |
| --- | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 100 users, 20/s | 100 | 0 | - | 135ms | 149ms | 32.61ms | 65.39ms | 37.86ms |
| 150 users, 20/s | 125 | 25 | timeout | 140ms | 15000ms | 32.05ms | 80.11ms | 37.37ms |
| 150 users, 20/s, nofile=65535 | 125 | 25 | timeout | 145ms | 15000ms | 32.56ms | 74.42ms | 39.46ms |
| 200 users, 20/s | 125 | 75 | timeout | 142ms | 15000ms | 32.41ms | 66.28ms | 36.73ms |
| 200 users, 40/s | 125 | 75 | timeout | 5237ms | 15000ms | 2849.95ms | 70.71ms | 36.49ms |

说明：

- 所有失败均为 `connect_timeout`，没有 resolve/tcp/ssl/ws 明确错误回调。
- 100 并发连接全成功，150 开始稳定卡在 125。
- 拉高发压机 shell `ulimit -n` 到 65535 后，150 连接仍然 `125/150`，说明不是发压进程 fd 上限。
- `200 users, 40/s` 的 TCP connect avg/p95 明显变差，说明更高突发速率会加重 TCP 建连排队/重传。

### 4.2 SUT 本机 loopback 对照

在 SUT 本机运行：

```bash
ulimit -n 65535
/tmp/bench_ws \
  --host 127.0.0.1 \
  --port 10001 \
  --tokens /tmp/users.json \
  --duration 20 \
  --interval 999999 \
  --users 150 \
  --threads 4 \
  --connect-rate 20
```

结果：

| 场景 | 成功 | 失败 | connect avg | TCP avg | TLS avg | WS avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| SUT local 150 users, 20/s | 150 | 0 | 2.80ms | 0.17ms | 2.63ms | 0.39ms |

这个对照非常关键：同一个 Gateway、同一个 `bench_ws`、同一份 token，在本机 loopback 下 150 连接全部成功。

## 5. Gateway 侧观测

压测后 `GET /api/v1/stats` 摘要：

```text
ws.accept_ok: 875
ws.accept_fail: 0
ws.active_handshakes: 0
ws.current_sessions: 0
ws.ssl_handshake.count: 875
ws.ssl_handshake.avg_ms: 38.8091
ws.ssl_handshake.max_ms: 382
ws.upgrade_read.count: 875
ws.upgrade_read.avg_ms: 31.7531
ws.upgrade_read.max_ms: 334
ws.accept_handshake.count: 875
ws.accept_handshake.avg_ms: 0.0754286
ws.accept_handshake.max_ms: 13
ws.session_add.count: 875
ws.session_add.avg_ms: 0.797714
ws.session_add.max_ms: 11
ws.inflight_messages: 0
thread_pool.queued_or_running_tasks: 0
```

解读：

- Gateway 应用层没有 accept 失败。
- 进入 Gateway 的连接，TLS/HTTP upgrade/WS accept/session_add 都能较快完成。
- 压测场景没有业务消息发送，`ws.inflight_messages=0`，所以本轮瓶颈与消息处理、DB 持久化、push fanout 无关。
- PVE 公网压测中失败的连接没有进入 Gateway 的 TLS/WS/业务统计，主要卡在进入应用 accept 之前的 TCP 建连路径。

SUT 内核计数中可见：

```text
failed connection attempts: 9656
resets received for embryonic SYN_RECV sockets: 9653
SYN cookies sent: 244
SYN cookies received: 246
times the listen queue of a socket overflowed: 2
SYNs to LISTEN sockets dropped: 2
TCPSynRetrans: 6440
```

这些计数包含历史值，不能直接等同于本轮单次压测增量，但方向上提示公网 TCP 建连路径存在重传、半连接 reset 或云侧策略影响。

## 6. 当前结论

当前 WSS 性能瓶颈已经不在以下模块：

- protobuf 编解码。
- Gateway WSS 业务消息处理。
- MessageService/DB 持久化。
- Push fanout。
- WebSocket close 生命周期。
- Gateway 进程 fd limit。
- `bench_ws` 原来的 IO worker 启动顺序。

目前最可信的定位是：

> PVE 发压机到云服务器公网链路上的 TCP 建连能力存在稳定阈值或策略限制，导致超过约 125 个新连接后，后续连接在客户端侧等待到 `15s` 超时，没有进入 Gateway 应用层。

证据链：

1. PVE 公网压 100 连接全成功。
2. PVE 公网压 150/200 连接稳定只有 125 成功。
3. 失败全部是 `connect_timeout`，不是 TLS/WS/业务错误。
4. 拉高 PVE nofile 到 65535 后仍是 125 成功。
5. SUT 本机 loopback 压 150 连接全成功，connect avg 只有 2.8ms。
6. Gateway stats 显示进入应用层的连接都能完成 TLS/WS/session_add。

## 7. 下一步建议

### 7.1 立即建议

短期不要继续盲目改消息处理链路，也不要把 `conn-200` 当作 Gateway 业务性能崩盘。

下一轮应优先做网络路径和连接入口专项：

1. 采集压测前后 `netstat -s` 增量，而不是只看累计值。
2. 使用另一台公网机器或同云厂商内网机器作为发压端复测。
3. 在 SUT 上用 `tcpdump` 抓 `tcp port 10001`，确认失败连接是否 SYN 到达、是否 SYN-ACK 返回、是否客户端 ACK 丢失。
4. 提高 SUT `net.ipv4.tcp_max_syn_backlog` 到 4096 或 8192 后复测公网建连。
5. 对比 `--connect-rate 5/10/20/40`，确认是否与突发速率强相关。
6. 将 `bench_ws --timeout` 临时提高到 30s，观察失败连接是最终成功还是持续不进入应用层。

### 7.2 Gateway 后续优化

如果网络路径排除后仍证明 Gateway accept 链路有瓶颈，再考虑：

1. 将 WSS acceptor 与 handshake/session IO 分离，不再把所有 accepted socket 的 TLS/WS 握手都绑定在 acceptor 所属的单个 `io_context` 上。
2. 给 `WebSocketServer` 增加 accept-loop 连续失败自恢复逻辑：accept 出错后仍继续 `do_accept()`，但需要避免关闭态无限重试。
3. 增加连接建立阶段分位数统计，而不只是 count/avg/max。
4. 将 `/api/v1/stats` 输出结构化 JSON，便于 benchmark 脚本自动归档。

## 8. 本轮验证命令

本地构建与测试：

```bash
cmake --build build/benchmark --target bench_ws -j2
cmake --build build/remote-push-odb --target gateway_server test_gateway_message_ws -j2
ctest --test-dir build/remote-push-odb -R GatewayMessageWsTest --output-on-failure
git diff --check
```

Debian 12 兼容版构建：

```bash
docker run --rm -v /home/myself/workspace/MyChat:/src -w /src/test/benchmark debian:12 bash -lc '...'
```

结果：

- `bench_ws` 构建通过。
- `gateway_server` 构建通过。
- `GatewayMessageWsTest` 通过。
- `git diff --check` 通过。
