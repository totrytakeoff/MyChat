# MyChat 压测工具

## 目录结构

```
test/benchmark/
├── bench_ws.cpp            WSS 压测工具 (C++, Boost.Beast)
├── http_benchmark.js       HTTP 压测脚本 (k6)
├── prep_users.py           批量注册/登录用户, 导出 token
├── run_all.py              一键运行全量压测
├── run_benchmark.sh        快捷启动脚本 (source env.sh 后运行)
├── pve_runner.py           SSH 远程执行工具库
├── deploy_to_pve.sh        部署工具到发压端
├── CMakeLists.txt          bench_ws 构建 (vcpkg 环境)
├── CMakeLists_deb12.txt    bench_ws 构建 (Debian 12, 系统包)
├── env.sh.example          环境变量模板
└── .gitignore
```

## 架构

```
┌──────────────┐          ┌──────────────────────────┐
│  发压端 (PVE) │  ──WSS─→ │  被测服务器 (SUT)         │
│              │          │                          │
│ bench_ws     │          │ gateway_server :10001     │
│ k6           │  ──HTTP→ │ gateway_server :10002     │
│ run_all.py   │          │  ├─ user service          │
│ (控制脚本)    │  ──SSH─→ │  ├─ message service       │
│              │          │  ├─ Redis :6379           │
└──────────────┘          │  └─ PostgreSQL :5432      │
       ↑                  └──────────────────────────┘
       │                            ↑
       └── SSH (key/password) ──────┘
              (监控/部署)
```

- **bench_ws**: C++ 异步 WebSocket 客户端, 建立 WSS 连接后按固定间隔发送 Protobuf 消息, 统计 RTT/吞吐/连接延迟
- **k6 + http_benchmark.js**: 阶梯式 HTTP 压测 (10→50→100→200→500 VUs), 测试 health 和 auth/info 端点
- **prep_users.py**: 通过 HTTP API 注册和登录用户, 导出 access_token 到 JSON
- **run_all.py**: 编排完整压测流程 (连接压力 → 消息吞吐 → 压力测试 → HTTP)

## 前提条件

### 被测服务器 (SUT)
- gateway_server 运行中, 监听 WSS + HTTP 端口
- PostgreSQL 和 Redis 可用
- SSH 可达

### 发压端 (Load Generator)
- Linux x86_64, Debian 12+ 或 Ubuntu 22.04+
- 依赖: `cmake g++ libboost-dev libssl-dev nlohmann-json3-dev libprotobuf-dev libspdlog-dev libabsl-dev pkg-config`
- Python 3.8+, pexpect (`pip3 install pexpect`)
- k6 v0.54+ ([安装指南](https://grafana.com/docs/k6/latest/set-up/install-k6/))

### 控制端 (执行 run_all.py 的机器)
- Python 3.8+, pexpect
- 可 SSH 到被测服务器和发压端

## 构建 bench_ws

### Debian 12 / Ubuntu 22.04 (系统包)
```bash
# 在发压端上
sudo apt install -y cmake g++ libboost-dev libssl-dev \
    nlohmann-json3-dev libprotobuf-dev libspdlog-dev \
    libabsl-dev pkg-config

cd test/benchmark
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DPROJECT_ROOT=../..
make -j$(nproc)
```

### 跨 glibc 构建 (如开发机 Arch → 发压端 Debian 12)
```bash
# 使用 Docker 在 Debian 12 环境中构建
docker run --rm -v $PWD/../../:/workspace debian:12-slim bash -c "
  apt-get update -qq && apt-get install -y -qq cmake g++ libboost-dev \
    libssl-dev nlohmann-json3-dev libprotobuf-dev libspdlog-dev \
    libabsl-dev pkg-config
  cd /workspace/test/benchmark && mkdir -p build_deb12 && cd build_deb12
  cmake .. -DCMAKE_BUILD_TYPE=Release -DPROJECT_ROOT=/workspace
  make -j4 bench_ws
"
# 产物: test/benchmark/build_deb12/bench_ws
```

## 配置

```bash
# 1. 创建配置
cd test/benchmark
cp env.sh.example env.sh

# 2. 编辑 env.sh, 填入实际值
vim env.sh

# 3. 加载配置
source env.sh
```

关键环境变量:

| 变量 | 说明 | 示例 |
|------|------|------|
| `BENCH_CLOUD_HOST` | 被测服务器 IP | `10.0.0.1` |
| `BENCH_CLOUD_SSH` | 被测服务器 SSH 连接串 | `root@10.0.0.1` |
| `BENCH_PVE_HOST` | 发压端 IP | `10.0.0.2` |
| `BENCH_PVE_USER` | 发压端 SSH 用户 | `root` |
| `BENCH_PVE_PASS` | 发压端 SSH 密码 (建议用密钥, 留空) | `""` |
| `BENCH_PVE_TOOL_DIR` | 发压端工具目录 | `/tmp/benchmark` |

## 准备压测用户

```bash
# 在发压端上运行, 注册 200 个用户并导出 token
python3 prep_users.py \
  --host ${BENCH_CLOUD_HOST} \
  --port ${BENCH_CLOUD_HTTP_PORT:-10002} \
  -n 200 \
  --password pass1234 \
  -o users.json

# 或通过 SSH 远程执行
ssh ${BENCH_PVE_USER}@${BENCH_PVE_HOST} \
  "python3 ${BENCH_PVE_TOOL_DIR}/prep_users.py \
   --host ${BENCH_CLOUD_HOST} \
   --port ${BENCH_CLOUD_HTTP_PORT:-10002} \
   -n 200 \
   --password pass1234 \
   -o ${BENCH_PVE_TOOL_DIR}/users.json"
```

产出 `users.json`:
```json
[
  {
    "uid": "uuid",
    "account": "bench_0001",
    "access_token": "eyJ...",
    "refresh_token": "...",
    "device_id": "bench-device",
    "platform": "bench"
  },
  ...
]
```

## 运行压测

### 一键运行全量测试
```bash
source env.sh
python3 run_all.py
```

测试阶段:
1. **连接压力** (conn): 10/50/100/200 用户纯连接, 无消息, 测试 SSL 握手吞吐
2. **轻量吞吐** (light): 10s 间隔, 测试低频消息场景
3. **中量吞吐** (medium): 2-5s 间隔, 测试中等消息频率
4. **压力测试** (stress): 250ms-1s 间隔, 寻找系统极限
5. **HTTP 阶梯** (HTTP): 10→50→100→200→500 VUs, 测试 REST 性能

### 选择性运行
```bash
# 只跑 WSS
python3 run_all.py --wss-only

# 只跑 HTTP
python3 run_all.py --http-only

# 只跑特定阶段
python3 run_all.py --phases conn,light      # 只跑连接+轻量
python3 run_all.py --phases stress           # 只跑压力测试
```

### 单独运行 bench_ws
```bash
# 在发压端
./bench_ws \
  --host <服务器IP> \
  --port 10001 \
  --tokens users.json \
  --users 50 \
  --duration 60 \
  --interval 5000 \
  --threads 4 \
  --connect-rate 20
```

### 单独运行 k6
```bash
# 在发压端
k6 run --vus 10 --duration 60s \
  -e BASE_URL=http://<服务器IP>:10002 \
  http_benchmark.js
```

HTTP 脚本支持拆分场景，便于定位是 HTTP 接入层还是认证业务路径慢：

```bash
# 只压健康检查
k6 run -e BASE_URL=http://<服务器IP>:10002 -e SCENARIO=health http_benchmark.js

# 只压认证用户信息
k6 run -e BASE_URL=http://<服务器IP>:10002 -e SCENARIO=info http_benchmark.js

# 混合场景，默认值
k6 run -e BASE_URL=http://<服务器IP>:10002 -e SCENARIO=mixed http_benchmark.js

# 禁用连接复用，对照 keep-alive 对 HTTP worker 的影响
k6 run -e BASE_URL=http://<服务器IP>:10002 -e SCENARIO=health \
  -e NO_CONNECTION_REUSE=true http_benchmark.js
```

## 日志和结果

压测日志保存在发压端的 `$BENCH_PVE_TOOL_DIR/logs/` 目录:

```
logs/
├── 20260619_154205_baseline.txt    # 压测前服务器状态
├── 20260619_154205_conn-10_pre.txt  # conn-10 测试前状态
├── 20260619_154205_conn-10.txt      # conn-10 bench_ws 输出
├── 20260619_154205_conn-10_post.txt # conn-10 测试后状态
├── ...
├── 20260619_154205_HTTP-Ramp.txt    # k6 HTTP 输出
└── 20260619_154205_final.txt        # 压测后服务器状态
```

## 结果解读

### bench_ws 输出示例
```
======= Results =======
Connects OK:    100        ← 成功连接数
Connects FAIL:  0          ← 失败连接数
Messages sent:  5900       ← 发送消息总数
Messages recv:  11800      ← 接收消息总数 (约为发送×2: 响应+推送)
Errors:         0          ← 解码/协议错误
Throughput:     98.3 msg/s ← 发送吞吐

Connect time:              ← 连接建立耗时 (SSL握手+WS升级)
  count: 100
  min/avg/max: 269 / 3311 / 65142 ms
  p50/p90/p95/p99: 2707 / 4727 / 4984 / 5149 ms

RTT:                       ← 消息往返延迟 (发送→收到响应)
  count: 5900
  min/avg/max: 31.50 / 57.27 / 356.02 ms
  p50/p90/p95/p99: 50.80 / 76.08 / 83.17 / 263.41 ms
```

### 关键指标解读

| 指标 | 含义 | 健康值 |
|------|------|--------|
| RTT p50 | 50% 的消息往返延迟 | < 100ms |
| RTT p95 | 95% 的消息往返延迟 | < 200ms |
| RTT p99 | 99% 的消息往返延迟 | < 500ms |
| Connect p50 | 50% 连接建立耗时 | < 500ms |
| Errors | 协议错误数 | 0 |
| Messages recv/sent | 接收/发送比 | ≈ 2.0 (每个发信收到响应+对端推送) |

### 异常信号

- **RTT 突然飙升**: 系统过载, 检查上游服务状态
- **Errors > 0**: 协议编解码问题或连接断开
- **recv/sent < 1.5**: 消息丢失, 可能是网关线程阻塞
- **Connects FAIL 持续增长**: SSL 握手瓶颈, 降低 connect-rate
- **系统负载 < 1 但吞吐不增**: 架构瓶颈 (同步 RPC, 线程池耗尽)

## bench_ws 参数参考

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--host` | 127.0.0.1 | 服务器地址 |
| `--port` | 10001 | WSS 端口 |
| `--tokens` | tokens.json | 用户 token 文件 |
| `--users` | 0 (全部) | 使用用户数 |
| `--duration` | 60 | 测试时长 (秒) |
| `--interval` | 5000 | 消息间隔 (毫秒) |
| `--timeout` | 15000 | 连接超时 (毫秒) |
| `--threads` | 4 | io_context 线程数 |
| `--connect-rate` | auto | 连接速率 (/秒) |
| `--csv` | false | CSV 输出模式 |

## prep_users.py 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--host` | 127.0.0.1 | API 服务器地址 |
| `--port` | 10002 | HTTP 端口 |
| `-n/--count` | 10 | 用户数量 |
| `--prefix` | bench | 账号前缀 (结果: bench_0001) |
| `--password` | pass1234 | 统一密码 |
| `-o/--output` | users.json | 输出文件 |
