# MyChat Gateway HTTP keep-alive 修复复测报告

生成时间: 2026-06-22 14:45 CST

## 结论

本轮复测确认 Gateway HTTP 长尾延迟问题已经被当前修复显著收敛。

在 `httplib::ThreadPool(64, 1024)`、`keep_alive_timeout=1s`、`keep_alive_max_count=1` 配置下，从 `lzr-host` 对云服务器 `122.51.70.33:10002` 发起 10 -> 50 -> 100 -> 200 -> 500 VUs 阶梯压测：

| 场景 | 请求数 | 失败率 | 平均延迟 | p90 | p95 | 最大延迟 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `GET /api/v1/health` | 23277 | 0.00% | 2.47ms | 2.65ms | 2.74ms | 14.24ms |
| `GET /api/v1/auth/info` | 23223 | 0.00% | 4.67ms | 5.60ms | 6.43ms | 111.83ms |

对比修复前和中间版本：

| 阶段 | 关键现象 |
| --- | --- |
| 修复前混合 HTTP 压测 | `http_req_failed=56.03%`，p95/p99 接近 60s，大量请求超时 |
| 显式线程池 + `keep_alive_max_count=10` | `/health` 不再失败，但 keep-alive 场景 p95 仍约 21.36s |
| 禁用客户端连接复用对照 | `/health` p95 约 2.71ms，证明长尾和 keep-alive 连接占用 worker 强相关 |
| `keep_alive_max_count=1` | 默认 keep-alive 压测下 `/health` p95 约 2.74ms，`/auth/info` p95 约 6.43ms |

## 定位判断

这次问题的核心不是认证、数据库、Redis 或业务 handler 本身慢，而是 cpp-httplib 同步 HTTP worker 被 keep-alive 空闲连接长期占用后，新请求在进入 handler 之前排队。

证据链如下：

1. 只压 `/api/v1/health` 也出现 20s 级 p95，说明问题不依赖业务链路。
2. 禁用 k6 连接复用后，`/health` p95 从约 21.36s 回到约 2.71ms。
3. Gateway `/api/v1/stats` 中 route handler 统计显示 `/health` 平均处理耗时接近 0ms，`/auth/info` 平均约 1.59ms。
4. 将服务端 `keep_alive_max_count` 收敛到 `1` 后，默认 keep-alive 客户端也回到毫秒级。

## 本轮修复内容

Gateway HTTP 初始化中明确配置：

- HTTP worker 数: `64`
- HTTP 排队上限: `1024`
- keep-alive timeout: `1s`
- keep-alive max count: `1`
- TCP_NODELAY: 启用
- `/api/v1/stats`: 输出 HTTP worker、队列、keep-alive 参数、状态码计数、路由请求数与 handler 耗时

压测脚本补充：

- `SCENARIO=health`
- `SCENARIO=info`
- `SCENARIO=mixed`
- `NO_CONNECTION_REUSE=true|false`

这些场景用于拆分“HTTP 接入层问题”和“业务路径问题”。

## 当前取舍

`keep_alive_max_count=1` 本质上是强制 HTTP 短连接化。它牺牲了 HTTP keep-alive 的连接复用收益，但换来了当前同步 httplib 模型下更稳定、可解释的排队行为。

对当前 MyChat MVP 和面试展示阶段，这是合理修复：

- WebSocket 才是 IM 实时链路主通道。
- HTTP 主要承担登录、资料、好友、群组、历史消息等 REST API。
- 当前瓶颈来自 HTTP server 模型，不是核心 IM 业务能力。
- 修复后 500 VUs 阶梯下 HTTP 基础接口稳定在毫秒级。

长期工程化优化方向：

1. 将 HTTP 接入层迁移到 Boost.Asio/Beast 或其他异步 HTTP server。
2. 对 HTTP worker、队列、keep-alive 参数做配置化，而不是硬编码。
3. 增加端到端排队耗时观测，例如 accept 到 handler、handler 到响应写回的分段指标。
4. 对登录等高成本接口单独压测，因为密码哈希路径明显比 `/auth/info` 更重。

## 原始日志

本轮原始日志保存在：

```text
docs/benchmark/benchmark_report_20260622_http_fix_keepalive1/raw_logs/
```

关键文件：

- `health_keepalive.txt`
- `info_keepalive.txt`
- `health_keepalive_pre_stats.txt`
- `health_keepalive_post_stats.txt`
- `info_keepalive_pre_stats.txt`
- `info_keepalive_post_stats.txt`

