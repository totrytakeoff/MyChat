# MyChat 完整压力测试报告

生成时间: 2026-06-22 15:20 CST

## 测试环境

- 被测服务器: `ASD-Host` / `122.51.70.33`
- 发压机器: `lzr-host`
- Gateway 端口:
  - WebSocket: `10001`
  - HTTP: `10002`
- Gateway 启动参数:
  - `/opt/mychat/bin/gateway_server -c /opt/mychat/config/benchmark.json -w 10001 -H 10002 -l info --max-open-files 65535`
- Gateway HTTP 参数:
  - `http.worker_threads=64`
  - `http.max_queued_requests=1024`
  - `http.keep_alive_timeout_sec=1`
  - `http.keep_alive_max_count=1`

## 总体结论

本轮完整压力测试确认 HTTP 接入层修复有效，WSS 连接、握手、常规消息压测整体稳定。

当前系统可以支撑：

- 200 并发 WSS 连接建立，连接失败为 0。
- 200 用户、1s 发送间隔的 WSS 消息压测，客户端错误为 0，RTT p95 约 12.60ms。
- 500 VUs HTTP 阶梯压测，失败率 0%，整体 p95 约 5.81ms。

当前暴露出的主要问题集中在极限 WSS 场景：

- `200 users / 250ms interval` 下，RTT p95 上升到约 2628.05ms。
- 该阶段出现 `Post-connect errors=1`。
- 服务端 Gateway 统计中 `accept_fail=0`、`parse.decode failed=0`、`parse.routing failed=0`，说明不是连接建立或消息解析失败，而更像是业务处理/推送回包链路在极限压力下发生排队。

## WSS 连接压力

| 场景 | 连接数 | 连接成功 | 连接失败 | 客户端错误 | 连接耗时 p95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `conn-10` | 10 | 10 | 0 | 0 | 13ms |
| `conn-50` | 50 | 50 | 0 | 0 | 13ms |
| `conn-100` | 100 | 100 | 0 | 0 | 14ms |
| `conn-200` | 200 | 200 | 0 | 0 | 14ms |

结论:

- WSS accept、TLS handshake、WebSocket upgrade 在 200 并发连接范围内稳定。
- Gateway 最终统计显示 `ws.accept_ok=1580`、`ws.accept_fail=0`。
- SSL 握手平均约 5ms，最大 32ms，属于可接受范围。

## WSS 消息压测

| 场景 | 连接数 | 间隔 | 发送 | 接收 | 错误 | 吞吐特征 | RTT p95 |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| `ws-10u-10s` | 10 | 10s | 59 | 118 | 0 | 低频 | 21.26ms |
| `ws-50u-10s` | 50 | 10s | 299 | 598 | 0 | 低频 | 11.00ms |
| `ws-100u-5s` | 100 | 5s | 1199 | 2398 | 0 | 轻量 | 9.73ms |
| `ws-50u-5s` | 50 | 5s | 599 | 1198 | 0 | 中量 | 9.45ms |
| `ws-100u-2s` | 100 | 2s | 3077 | 6154 | 0 | 中量 | 10.14ms |
| `ws-200u-5s` | 200 | 5s | 2498 | 4996 | 0 | 中量 | 10.39ms |
| `ws-100u-1s` | 100 | 1s | 6195 | 12390 | 0 | 压力 | 11.00ms |
| `ws-200u-1s` | 200 | 1s | 12890 | 25780 | 0 | 压力 | 12.60ms |
| `ws-200u-500ms` | 200 | 500ms | 13880 | 27760 | 0 | 高压 | 21.92ms |
| `ws-200u-250ms` | 200 | 250ms | 27858 | 51291 | 1 | 极限 | 2628.05ms |

结论:

- 200 用户、1s 间隔以内表现稳定，RTT p95 仍在十几毫秒级。
- 200 用户、500ms 间隔已经出现可见抖动，但仍没有客户端错误。
- 200 用户、250ms 间隔进入当前系统极限区间，RTT 出现秒级堆积，说明某条异步处理或推送回包链路开始排队。

需要注意:

- `Messages recv` 约为 `Messages sent * 2`，符合当前压测工具/回包模型表现。
- `processed websocket message count=68554`，服务端处理计数与客户端收发规模同量级。
- `parse.decode failed=0`、`parse.routing failed=0`，当前不是协议解码和路由配置问题。

## HTTP 阶梯压测

HTTP 场景为 10 -> 50 -> 100 -> 200 -> 500 VUs，默认混合请求 `/api/v1/health` 与 `/api/v1/auth/info`。

| 指标 | 数值 |
| --- | ---: |
| 总请求数 | 46230 |
| 失败率 | 0.00% |
| HTTP 平均延迟 | 3.52ms |
| HTTP p90 | 5.08ms |
| HTTP p95 | 5.81ms |
| HTTP 最大延迟 | 19.88ms |
| 吞吐 | 306.24 req/s |

分接口:

| 接口 | 平均 | p90 | p95 | 最大 |
| --- | ---: | ---: | ---: | ---: |
| `/api/v1/health` | 2.50ms | 2.73ms | 2.96ms | 14.46ms |
| `/api/v1/auth/info` | 4.55ms | 5.79ms | 6.61ms | 19.89ms |

结论:

- HTTP keep-alive worker 占用问题已收敛。
- 500 VUs 阶梯压测没有 4xx/5xx 污染，失败率为 0。
- Gateway route handler 统计显示 `/auth/info` 平均约 1.53ms，端到端 k6 p95 约 6.61ms，符合网络和客户端调度开销预期。

## Gateway 最终状态

压测结束后 Gateway `/api/v1/stats` 关键值:

```text
ws.accept_ok: 1580
ws.accept_fail: 0
ws.current_sessions: 0
ws.ssl_handshake.avg_ms: 5.04557
ws.ssl_handshake.max_ms: 32
processed websocket message count:68554
parse.decode failed count: 0
parse.routing failed count:0
ws.inflight_messages: 0
http.total_requests: 93146
http.status_2xx: 92945
http.status_4xx: 200
http.status_5xx: 0
```

`http.status_4xx=200` 来自本轮前置用户注册/登录和历史压测遗留统计，不是本轮 HTTP ramp 的失败。k6 本轮 HTTP ramp 统计为 `0 out of 46230` failed。

## 暴露问题

### 1. WSS 极限场景 RTT 秒级堆积

触发场景:

```text
200 users / 250ms interval / 30s
```

现象:

```text
Messages sent: 27858
Messages recv: 51291
Errors: 1
RTT avg: 1261.98ms
RTT p95: 2628.05ms
RTT p99: 2765.45ms
```

初步判断:

- 连接层正常，握手和 accept 没有失败。
- 协议解析正常，decode/routing failed 为 0。
- 问题更可能在消息处理、持久化、推送回包、线程池任务排队、单连接写队列或 bench 客户端接收处理能力之间。

下一步定位建议:

1. 给 WSS 消息处理链路补端到端分段指标:
   - 收包到解析完成
   - 解析到业务处理完成
   - 业务处理到 push 投递
   - push 投递到 session 写入队列
   - 写队列长度和写完成耗时
2. 在 `ws-200u-250ms` 单独复测时同步采集:
   - Gateway CPU、线程数、上下文切换
   - Redis/PostgreSQL 慢查询或连接池等待
   - 线程池队列深度
   - 每连接 pending write 数
3. 对比 `200u-500ms` 与 `200u-250ms` 的服务端指标差异，定位拐点。

### 2. benchmark 编排脚本等待逻辑已修复

本轮发现 `test/benchmark/run_all.py` 的 `pve()` 只等待 10 秒判断 SSH 是否进入 password/EOF。远端压测命令正常运行超过 10 秒时，本地 pexpect 会误判 timeout。

已修改为按阶段 timeout 等待 EOF:

```python
idx = child.expect_exact(["password: ", pexpect.EOF], timeout=timeout)
```

这属于压测工具链修复，需要在下一次提交中纳入。

## 后续建议

短期:

1. 保留当前 HTTP 修复结论，HTTP 暂不继续深挖。
2. 将性能定位重点转向 WSS 极限消息链路。
3. 单独复测 `ws-200u-250ms`，不要每次跑全量，节省定位时间。
4. 增加服务端分段耗时和队列观测后再判断是否需要改业务链路。

中期:

1. 压测脚本输出改为结构化 JSON 或 CSV，避免人工从文本里抽指标。
2. `run_all.py` 的日志目录语义需要修正，当前 `OUTDIR` 是控制端本地路径，不是发压端远程路径。
3. 将 Gateway HTTP 参数配置化。
4. 对 WSS 压测增加固定阈值，超过 p95 或错误数时自动标红。

## 原始日志

原始日志保存在:

```text
docs/benchmark/benchmark_report_20260622_full_retest/raw_logs/
```

关键文件:

- `20260622_145729_ws-200u-250ms.txt`
- `20260622_145729_ws-200u-500ms.txt`
- `20260622_145729_ws-200u-1s.txt`
- `20260622_151056_HTTP-Ramp.txt`
- `20260622_145729_final.txt`
- `20260622_151056_final.txt`

