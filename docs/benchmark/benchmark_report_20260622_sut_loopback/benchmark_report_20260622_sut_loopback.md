# 2026-06-22 SUT 本机 WSS Loopback 高强度建连复测

## 1. 测试目的

上一轮公网压测显示：

- PVE 发压机经公网访问 SUT WSS，`150/200 users` 稳定只有 `125` 个连接成功。
- 失败全部是客户端 `connect_timeout`。
- Gateway 应用层只统计到成功进入的连接，TLS/WS/session_add 本身没有明显慢点。

本轮进一步把 `bench_ws` 直接放到 SUT 本机，通过 `127.0.0.1:10001` 压测 Gateway，完全绕开公网链路、云安全组、运营商 NAT 和单源公网入口策略，用来确认瓶颈是否来自 Gateway 自身。

## 2. 测试环境

SUT:

- Host: `122.51.70.33`
- SSH alias: `ASD-Host`
- Gateway PID: `2819176`
- Gateway WSS: `127.0.0.1:10001`
- Gateway HTTP stats: `127.0.0.1:10002/api/v1/stats`

本机压测工具：

```text
/tmp/bench_ws
/tmp/users.json
```

可用测试用户：

```text
200
```

命令模板：

```bash
ulimit -n 65535
/tmp/bench_ws \
  --host 127.0.0.1 \
  --port 10001 \
  --tokens /tmp/users.json \
  --duration 20 \
  --interval 999999 \
  --users 200 \
  --threads 4 \
  --connect-rate <rate>
```

原始日志：

- `docs/benchmark/benchmark_report_20260622_sut_loopback/raw_logs/`

## 3. 测试矩阵与结果

| 场景 | 成功 | 失败 | connect avg | connect p95 | TCP avg | TLS avg | WS avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| loopback 200 users, 20/s | 200 | 0 | 3.07ms | 4ms | 0.17ms | 2.78ms | 0.47ms |
| loopback 200 users, 40/s | 200 | 0 | 2.73ms | 4ms | 0.15ms | 2.67ms | 0.41ms |
| loopback 200 users, 80/s | 200 | 0 | 2.35ms | 3ms | 0.19ms | 2.38ms | 0.33ms |
| loopback 200 users, 200/s | 200 | 0 | 3.46ms | 11ms | 0.65ms | 2.79ms | 0.49ms |

合计：

```text
4 轮 x 200 连接 = 800 次本机 WSS 建连
成功: 800
失败: 0
```

压测结束后 socket 状态：

```text
LISTEN    1
TIME-WAIT 400
```

未出现新的 `ESTAB` 或 `CLOSE-WAIT` 残留。

## 4. Gateway Stats 摘要

本轮开始前：

```text
ws.accept_ok: 900
ws.accept_fail: 0
ws.current_sessions: 0
```

本轮结束后：

```text
ws.accept_ok: 1700
ws.accept_fail: 0
ws.active_handshakes: 0
ws.current_sessions: 0
ws.ssl_handshake.count: 1700
ws.upgrade_read.count: 1677
ws.accept_handshake.count: 1677
ws.session_add.count: 1675
ws.inflight_messages: 0
thread_pool.queued_or_running_tasks: 0
```

`accept_ok` 从 `900` 增加到 `1700`，刚好增加 `800`，与四轮本机压测连接成功数一致。

## 5. 结论

本轮结果进一步确认：

1. Gateway 本机 WSS 建连能力可以稳定覆盖当前 200 用户测试集。
2. 在 `connect-rate=200/s` 的本机突发建连下，Gateway 仍然 `200/200` 成功。
3. 连接耗时主要是 TLS handshake，平均约 `2.4ms ~ 2.8ms`，不是 Gateway 业务逻辑、DB、push、protobuf 或 session 注册拖慢。
4. 生命周期修复有效，压测结束后无新的 `CLOSE-WAIT/ESTAB` 残留。
5. 与公网 PVE 压测 `150/200 users` 稳定卡 `125` 的结果对比，当前最可信瓶颈在公网入口链路或云服务商/安全策略对单源 IP 新建连接的限制，而不是 Gateway 应用层。

更直接地说：

> 如果问题来自 Gateway 自身，那么 SUT 本机 200 用户、最高 200/s 建连也应该复现失败；但实际本机 800 次建连全部成功。因此公网单源 IP 限制、云防护策略、NAT/链路丢包或公网 TCP 半连接路径才是下一轮应优先排查的方向。

## 6. 后续建议

下一步不建议继续盲改 Gateway 消息链路。建议按以下顺序验证公网入口假设：

1. 换第二台公网发压机复测同一 SUT。
2. 两台发压机同时各压 100，看总成功数能否超过单机公网的 125 阈值。
3. 在 SUT 上 `tcpdump -i any tcp port 10001`，确认失败连接的 SYN/SYN-ACK/ACK 是否完整。
4. 压测前后采集 `netstat -s` 增量，而不是累计值。
5. 临时调大 SUT `net.ipv4.tcp_max_syn_backlog` 到 `4096/8192`，再跑公网单源复测。
6. 联系云厂商或检查安全组/云防火墙/DDoS 防护策略中是否存在单源 IP 新建连接速率或并发连接限制。
