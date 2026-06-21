# 2026-06-21 Gateway WSS 生命周期修复复测报告

## 结论

本轮部署最新 `gateway_server` 后，WSS 连接生命周期问题已明显收敛：

- 压测结束后 Gateway 进程存活。
- WSS 端口最终只剩 `LISTEN`，未再残留 `CLOSE-WAIT` 或 `ESTAB`。
- `conn-10/50/100` 连接场景全部成功。
- `conn-200` 仍有 75 个连接失败，说明连接释放问题已修，但高并发建连性能仍需单独优化。

这轮复测的核心验收目标是确认 `WebSocketSession` 统一关闭、idle timeout、keepalive ping、`CMD_HEARTBEAT` 之后，异常连接是否还会污染后续压测。当前结果显示：连接释放链路通过验收，后续可以把注意力转向连接建立性能。

## 部署信息

被测机器：

- SUT: `122.51.70.33`，SSH alias: `ASD-Host`
- WSS: `10001`
- HTTP: `10002`
- Gateway PID: `2800884`

Gateway 启动命令：

```bash
/opt/mychat/bin/gateway_server \
  -c /opt/mychat/config/benchmark.json \
  -w 10001 \
  -H 10002 \
  -l info \
  --max-open-files 65535
```

部署后二进制 hash：

```text
56788699d22ea308d350af0bcca9a254e32fff536cd648fe03bf019f3abc1450  /opt/mychat/bin/gateway_server
```

监听与 fd 配置：

```text
LISTEN backlog: 4096 on 10001/10002
Max open files: soft 65535, hard 524288
```

## 压测工具说明

本机直接构建的 `bench_ws` 依赖 `GLIBC_2.38`，发压机 Debian 12 只有 `GLIBC_2.36`，无法运行。因此本轮使用 Debian 12 容器重新生成 protobuf 并构建兼容版压测工具：

```bash
cmake -S test/benchmark -B /tmp/mychat-bench-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DPROJECT_ROOT=/src \
  -DMYCHAT_BENCH_REGENERATE_PROTO=ON
cmake --build /tmp/mychat-bench-build --target bench_ws -j2
```

发压机验证：

```text
/tmp/benchmark/bench_ws -> runs successfully
libprotobuf.so.32 / libssl.so.3 / glibc 2.36 compatible
```

## 测试命令

```bash
export BENCH_CLOUD_HOST=122.51.70.33
export BENCH_CLOUD_SSH=ASD-Host
export BENCH_CLOUD_WSS_PORT=10001
export BENCH_CLOUD_HTTP_PORT=10002
export BENCH_PVE_HOST=192.168.1.185
export BENCH_PVE_USER=root
export BENCH_PVE_PASS=<set locally when password auth is required>
export BENCH_PVE_TOOL_DIR=/tmp/benchmark

python3 test/benchmark/run_all.py --wss-only --phases conn
```

Raw logs:

- `docs/benchmark/benchmark_report_20260621_ws_lifecycle_retest/raw_logs/local_run_all/`

## 连接压力结果

| 场景 | 成功 | 失败 | 成功率 | connect avg | connect p95 | connect p99 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| conn-10 | 10 | 0 | 100% | 372.60 ms | 603 ms | 603 ms |
| conn-50 | 50 | 0 | 100% | 1448.56 ms | 2541 ms | 2670 ms |
| conn-100 | 100 | 0 | 100% | 2763.23 ms | 5037 ms | 5289 ms |
| conn-200 | 125 | 75 | 62.5% | 10046.36 ms | 15000 ms | 15000 ms |

对比上一轮连接复测：

| 场景 | 上一轮成功 | 本轮成功 | 变化 |
| --- | ---: | ---: | ---: |
| conn-10 | 10/10 | 10/10 | 持平 |
| conn-50 | 48/50 | 50/50 | 提升 |
| conn-100 | 100/100 | 100/100 | 持平 |
| conn-200 | 99/200 | 125/200 | 提升，但仍不达标 |

## 连接状态验收

压测前：

```text
1 LISTEN
```

压测中 `conn-100` 观察到：

```text
100 ESTAB
1 LISTEN
```

压测结束并等待 idle timeout 后：

```text
1 LISTEN
```

最终 Gateway 状态：

```text
PID 2800884
RSS 44804 KB
WSS/HTTP listen backlog 4096
Max open files soft 65535
```

验收判断：

- `CLOSE-WAIT=0`：通过。
- 无活跃客户端时 `ESTAB` 回落到 0：通过。
- Gateway 进程存活且资源稳定：通过。
- `conn-200` 成功率：不通过，需要单独优化建连能力。

## 问题分析

本轮验证说明之前的连接残留问题主要由两类问题造成：

1. 关闭路径没有统一兜底，导致异常关闭后 socket/session 清理不完整。
2. 缺少 idle timeout/keepalive，客户端异常退出或半开时服务端无法及时释放连接。

这些问题在本轮已经被 `WebSocketSession::perform_close()`、Beast idle timeout、keepalive ping 收敛。

但 `conn-200` 仍然失败 75 个连接，且 p95/p99 均到达 15000ms timeout。这说明剩余瓶颈不再是“连接关不干净”，而是“连接建得慢或建不上”。可能方向包括：

- TLS 握手在单机高并发建连下排队明显。
- `bench_ws --connect-rate 20` 下，200 用户在 30 秒窗口内仍有一批连接触及 15s 超时。
- Gateway accept/SSL/WebSocket handshake 链路可能需要更细粒度统计。
- 发压端单进程/线程模型也可能参与瓶颈，需要用更小阶梯和不同 connect-rate 拆分验证。

## 下一步建议

1. 先把 WSS 生命周期修复视为通过，不再继续卡在 `CLOSE-WAIT/ESTAB` 残留问题上。
2. 针对 `conn-200` 做建连专项复测：
   - `200 users, connect-rate=10/s`
   - `200 users, connect-rate=20/s`
   - `200 users, connect-rate=40/s`
   - `300 users, connect-rate=20/s`
3. 在 Gateway 侧补 handshake/accept 观测：
   - SSL handshake 耗时
   - HTTP upgrade read 耗时
   - WebSocket accept 耗时
   - 当前 active handshakes 数
4. 视结果决定是否优化 TLS/accept 线程模型，或将建连能力作为后续性能专项，不阻塞当前项目总结和面试准备。
