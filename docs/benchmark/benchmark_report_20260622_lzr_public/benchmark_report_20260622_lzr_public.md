# 2026-06-22 lzr-host 公网 WSS 建连复测

## 1. 测试目的

上一轮 PVE 发压机经公网访问 SUT 时，`150/200 users` 稳定只有 `125` 个连接成功；而 SUT 本机 loopback `200 users @ 20/40/80/200/s` 全部成功。

本轮使用另一台云服务器 `lzr-host` 作为公网发压端，验证 `125` 阈值是否是 SUT 云服务器入口的普遍单源 IP 限制，还是 PVE 发压机自身公网出口、源 IP、NAT 或链路问题。

## 2. 测试环境

发压端：

- SSH alias: `lzr-host`
- Hostname: `VM-4-10-debian`
- OS: Debian 12
- glibc: `2.36`
- `ulimit -n`: `65535`

被测端：

- SUT: `122.51.70.33`
- WSS: `122.51.70.33:10001`
- Gateway stats: `http://127.0.0.1:10002/api/v1/stats`

压测工具：

```text
/tmp/bench_ws
0986f6755264ed053ca4308c6b4ab2fd615be63a52b4fff32e4dc0d98057e379
```

测试用户：

```text
/tmp/users.json
users: 200
```

原始日志：

- `docs/benchmark/benchmark_report_20260622_lzr_public/raw_logs/`

## 3. 运行说明

`lzr-host` 初始缺少 `libprotobuf.so.32`，已安装 Debian 12 运行依赖：

```bash
sudo apt-get install -y --no-install-recommends libprotobuf32 libssl3 ca-certificates
```

压测命令模板：

```bash
ulimit -n 65535
/tmp/bench_ws \
  --host 122.51.70.33 \
  --port 10001 \
  --tokens /tmp/users.json \
  --duration 20 \
  --interval 999999 \
  --users 200 \
  --threads 4 \
  --connect-rate <rate>
```

## 4. 测试矩阵与结果

| 场景 | 成功 | 失败 | connect avg | connect p95 | TCP avg | TLS avg | WS avg |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| lzr public 200 users, 20/s | 200 | 0 | 13.29ms | 18ms | 2.38ms | 7.75ms | 3.55ms |
| lzr public 200 users, 40/s | 200 | 0 | 11.99ms | 14ms | 2.31ms | 7.52ms | 2.61ms |
| lzr public 200 users, 80/s | 200 | 0 | 13.30ms | 18ms | 2.32ms | 8.43ms | 3.00ms |
| lzr public 200 users, 200/s | 200 | 0 | 17.30ms | 33ms | 2.56ms | 11.46ms | 3.73ms |

合计：

```text
4 轮 x 200 连接 = 800 次公网 WSS 建连
成功: 800
失败: 0
```

`lzr-host` 侧压测结束后：

```text
TIME-WAIT 400
```

SUT 侧压测结束后：

```text
ws.accept_ok: 2500
ws.accept_fail: 0
ws.active_handshakes: 0
ws.current_sessions: 0
ws.inflight_messages: 0
thread_pool.queued_or_running_tasks: 0
LISTEN 1
TIME-WAIT 121
```

本轮开始前 SUT `ws.accept_ok` 为 `1700`，结束后为 `2500`，刚好增加 `800`，与 `lzr-host` 四轮成功连接数一致。

## 5. 三方对照结论

| 发压来源 | 路径 | 代表场景 | 结果 |
| --- | --- | --- | --- |
| PVE `192.168.1.185` | 公网到 SUT | 200 users, 20/s | 125/200 |
| SUT 本机 | loopback | 200 users, 200/s | 200/200 |
| `lzr-host` | 公网到 SUT | 200 users, 200/s | 200/200 |

这个对照说明：

1. Gateway 应用层不是当前建连瓶颈。
2. SUT 所在云服务器并不存在“所有单源公网 IP 都只能建 125 个连接”的普遍限制。
3. PVE 发压机出现的 `125` 阈值更可能来自 PVE 自身公网出口、源 IP、NAT、运营商链路、路由路径或该源 IP 在云侧安全策略中的特殊限流。
4. `lzr-host` 公网路径能稳定做到 `200 users @ 200/s`，说明后续需要用更多源 IP/路径做压测，而不是继续盲改 Gateway 业务链路。

## 6. 后续建议

1. 保留 SUT 本机 loopback 和 `lzr-host` 作为基准发压路径。
2. 对 PVE 路径做专项网络排查：
   - 压测前后采集 PVE 与 SUT 的 `netstat -s` 增量。
   - 在 SUT 上 `tcpdump -i any tcp port 10001`，确认 PVE 失败连接的 SYN/SYN-ACK/ACK。
   - 检查 PVE 出口 NAT、路由器、防火墙和运营商链路。
3. 如需更高并发，优先补更多测试用户，然后在 `lzr-host` 和 SUT loopback 上跑 `500/1000` 用户矩阵。
4. benchmark 脚本应支持多发压端统一调度和自动归档 stats/ss/netstat。
