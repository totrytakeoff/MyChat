# MyChat WSS 压测复测报告

**日期**: 2026-06-21 14:48:53 ~ 14:53:10 CST
**目的**: 验证 gateway WebSocket dispatch/backpressure、`ulimit`、HTTP/WSS listen backlog 修复后的稳定性表现
**被测系统 (SUT)**: 122.51.70.33 (`VM-0-5-debian`, 4C/7.5GiB)
**发压端**: 192.168.1.185 (`pve0`, root)
**代码基线**: `e80979d Stabilize gateway ws dispatch and proto generation`
**gateway 二进制 SHA256**: `2e250c2706506411ae55c36ecabe80c1615b355d3836891acf3eb9116b7d2d19`
**配置 SHA256**: `559279c1a5b1dd0c6a4bb91c8df5789dba10ff83cf40f16b11f5526011f76433`

---

## 1. 本轮变更与环境

| 项目 | 本轮状态 |
|------|----------|
| gateway 启动参数 | `/opt/mychat/bin/gateway_server -c /opt/mychat/config/benchmark.json -w 10001 -H 10002 -l info --max-open-files 65535` |
| 文件描述符限制 | `Max open files 65535/65535` |
| WSS 监听 | `0.0.0.0:10001`, backlog `4096` |
| HTTP 监听 | `0.0.0.0:10002`, backlog `4096` |
| Redis | `127.0.0.1:6379`, 无密码 |
| PostgreSQL | `127.0.0.1:5432`, database `mychat` |
| 压测用户 | 200 个: `bench_0001` ~ `bench_0200` |
| 压测工具 | `bench_ws`, C++/Boost.Beast, 4 io threads, connect-rate 20/s |

本轮压测只跑 WSS stress 阶段，用于快速复测 2026-06-19 报告中 `ws-200u-250ms` 导致 gateway 崩溃的问题。

原始日志已归档:

- `raw_logs/local_run_all/`: `run_all.py` 生成的阶段输出与 SUT 资源快照
- `raw_logs/sut/`: gateway 启动日志、WebSocket/session/message handler 日志、压测后进程状态

> 备注: `run_all.py` 当前会把阶段日志写在执行端本机 `/tmp/benchmark/logs`，不是发压机远端目录。因此本报告将本机日志归档到 `raw_logs/local_run_all/`。这是 benchmark 工具链后续需要修正的小问题。

---

## 2. 压测结果

| 场景 | 用户 | 间隔 | 目标时长 | 实际耗时 | 连接成功 | 连接失败 | 发送 | 接收 | 错误 | 发送吞吐 | RTT avg | RTT p50 | RTT p95 | RTT p99 |
|------|------|------|----------|----------|----------|----------|------|------|------|----------|---------|---------|---------|---------|
| `ws-100u-1s` | 100 | 1000ms | 60s | 71s | 100 | 1 | 5,900 | 11,800 | 0 | 83.1/s | 52.75ms | 50.63ms | 76.08ms | 87.01ms |
| `ws-200u-1s` | 200 | 1000ms | 60s | 77s | 125 | 79 | 7,375 | 14,337 | 0 | 95.8/s | 63.37ms | 61.14ms | 94.64ms | 114.65ms |
| `ws-200u-500ms` | 200 | 500ms | 30s | 47s | 125 | 79 | 7,375 | 14,042 | 0 | 156.9/s | 53.27ms | 49.10ms | 81.60ms | 111.53ms |
| `ws-200u-250ms` | 200 | 250ms | 30s | 47s | 125 | 82 | 14,750 | 28,084 | 0 | 313.8/s | 77.47ms | 72.66ms | 122.04ms | 143.24ms |

连接耗时:

| 场景 | Connect avg | Connect p50 | Connect p90 | Connect p95 | Connect p99 | Connect max |
|------|-------------|-------------|-------------|-------------|-------------|-------------|
| `ws-100u-1s` | 3,372.80ms | 2,795ms | 4,824ms | 5,033ms | 5,233ms | 65,386ms |
| `ws-200u-1s` | 11,216.22ms | 9,147ms | 15,000ms | 15,000ms | 70,688ms | 70,771ms |
| `ws-200u-500ms` | 10,650.19ms | 9,145ms | 15,000ms | 15,000ms | 40,401ms | 41,159ms |
| `ws-200u-250ms` | 11,047.99ms | 9,144ms | 15,000ms | 15,000ms | 40,868ms | 41,011ms |

---

## 3. 与 2026-06-19 结果对比

| 指标 | 2026-06-19 `ws-200u-250ms` | 2026-06-21 `ws-200u-250ms` | 结论 |
|------|----------------------------|----------------------------|------|
| gateway 稳定性 | 崩溃退出 | 压测后仍存活 | 核心稳定性问题已修复 |
| bench 错误数 | 200 | 0 | 错误消除 |
| 发送消息 | 23,102 | 14,750 | 本轮仅 125 个连接成功，发送总量不可直接横比 |
| 接收消息 | 13,068 | 28,084 | 已不再出现接收侧大面积断崖式丢失 |
| RTT avg | 6,262ms | 77.47ms | 消息处理延迟恢复正常区间 |
| RTT p95 | 12,102ms | 122.04ms | 高分位延迟显著改善 |
| 进程状态 | 需手动重启 | `pid=2607613` 持续运行 | 通过稳定性复测 |

本轮最重要结论: 之前 `200 users × 250ms` 会触发 gateway 崩溃；修复后同档位不再崩溃，且业务消息 RTT 保持在百毫秒级。

---

## 4. 资源表现

压测前:

| 指标 | 数值 |
|------|------|
| load average | `0.15, 0.14, 0.09` |
| 内存 | `849Mi used / 7.5Gi total` |
| gateway CPU/MEM | `1.6% CPU / 0.2% MEM` |

压测结束:

| 指标 | 数值 |
|------|------|
| load average | `0.69, 0.43, 0.21` |
| 内存 | `875Mi used / 7.5Gi total` |
| gateway 进程 | `pid=2607613`, `3.8% CPU / 0.3% MEM`, RSS `31192 KiB` |

压测后 15:14 复查:

| 指标 | 数值 |
|------|------|
| gateway 存活 | 是 |
| open files limit | `65535/65535` |
| WSS backlog | `4096` |
| HTTP backlog | `4096` |
| load average | `0.06, 0.04, 0.07` |
| 可用内存 | `6.7Gi` |

服务器资源仍然明显未打满。当前 200 用户场景的问题更像连接建立/握手阶段排队或测试工具连接管理问题，而不是 CPU、内存或消息处理路径本身耗尽。

---

## 5. 关键观察

### 5.1 已修复的问题

1. `ulimit -n` 已从旧进程的 `1024` 提升到 `65535`。
2. HTTP listen backlog 已从旧进程的 `5` 提升到 `4096`。
3. WSS listen backlog 保持 `4096`。
4. `ws-200u-250ms` 不再触发 gateway crash。
5. 高压消息处理阶段没有出现 `Errors`，RTT 回到可接受范围。

### 5.2 仍然存在的问题

1. 200 用户场景实际只稳定连上约 125 个连接。
2. 连接阶段耗时很高，`Connect p90/p95` 达到 15s 超时阈值。
3. `Connect max` 出现 40s~70s 长尾，说明连接完成事件和超时统计之间仍可能存在 benchmark 工具侧竞态或连接关闭滞后。
4. 本轮 `run_all.py` 日志落点和注释不一致，远端执行输出保存到了本机 `/tmp/benchmark/logs`。

### 5.3 对当前瓶颈的判断

消息链路本身已经明显恢复稳定:

- `ws-200u-250ms` RTT avg `77.47ms`
- RTT p95 `122.04ms`
- `Errors=0`
- gateway 未崩溃

但连接建立链路仍不理想:

- 100 用户能全部连上
- 200 用户只连上约 125 个
- p90/p95 卡在 15s，说明 200 连接建链阶段有明显排队或超时

优先怀疑方向:

1. 发压端 `bench_ws` 以单进程 4 io threads 同时处理大量 TLS handshake，客户端侧可能先成为瓶颈。
2. gateway WSS accept/SSL handshake 处理线程或 strand 调度仍有排队。
3. 跨公网连接建立本身有长尾，20 conn/s 对 TLS 握手仍偏激进。
4. benchmark 的连接失败统计仍可能存在“超时后连接最终成功”的误计数问题，需要进一步修正统计模型。

---

## 6. 结论

本轮复测证明此前最严重的 gateway crash 问题已经被修复。`ws-200u-250ms` 不再把进程打崩，消息 RTT 从秒级/十秒级恢复到百毫秒以内，且压测后 gateway 持续存活。

项目当前不应再卡在“会不会直接崩”的阶段。下一步性能工作应从稳定性修复转向连接建立链路分析，重点拆分客户端发压瓶颈、服务端 TLS handshake/accept 排队、以及 benchmark 统计模型三件事。

建议后续单独做三组验证:

1. 固定 200 用户，降低 connect-rate: 5/s、10/s、20/s，对比连接成功率和 connect p95。
2. 固定 125 用户，跑 250ms/100ms 消息间隔，确认消息处理链路真实吞吐上限。
3. 增加 benchmark 连接失败原因分类: resolve/tcp/ssl/ws/timeout，并修复 timeout 后最终成功的重复统计。

---

## 7. 本轮 benchmark 工具修复记录

为了能在发压机旧 glibc 环境运行，本轮对 `test/benchmark` 做了工程化修复:

1. `bench_ws` 不再直接链接业务侧 `common/network/protobuf_codec.cpp` 和 `LogManager`，改用 benchmark 专用 `BenchmarkCodec`。
2. `BenchmarkCodec` 保持与业务协议一致的 wire format:
   `[header_size(varint)][type_name_size(varint)][type_name][header_data][message_data][CRC32]`
3. `test/benchmark/CMakeLists.txt` 增加 `MYCHAT_BENCH_REGENERATE_PROTO`:
   - 默认本地开发仍使用仓库 `common/proto/*.pb.cc`
   - 低版本发压环境可用系统 protoc 重新生成 benchmark 专用 pb 源码
4. Debian 12 兼容产物已验证可在发压机运行。

这些改动只影响压测工具，不改变 gateway 业务逻辑。
