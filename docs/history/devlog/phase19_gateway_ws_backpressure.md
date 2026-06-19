# Phase 19: Gateway WebSocket Backpressure And Dispatch Cleanup

Date: 2026-06-19

## Background

本阶段来自压测后的 Gateway WebSocket 链路排查。压测现象显示：WSS 在中等负载下可稳定工作，但在更高频消息场景下 RTT 快速进入秒级并出现错误。问题不是单个算法慢，而是入口参数、消息调度模型和同步业务链路叠加造成的排队与资源消耗。

本轮先收敛最直接的调度问题：Gateway WSS 正常消息路径不能再为每条消息创建独立异步线程和等待线程。

## Fixed Scope

### 1. MessageProcessor 调度模型

修改文件：

- `gateway/message_processor/message_processor.cpp`
- `gateway/message_processor/message_processor.hpp`

修复前：

```text
MessageProcessor::process_message
-> std::async(std::launch::async)
-> 每条消息可能触发运行时创建独立异步执行线程
```

修复后：

```text
MessageProcessor::process_message
-> ThreadPool::GetInstance().Enqueue(...)
-> process_message_sync(...)
```

新增 `process_message_sync()`，把原来的认证、cmd_id 分发、handler 调用、异常处理收敛为同步核心。异步入口只负责把同步核心投递到线程池。

### 2. Gateway WSS 回调路径

修改文件：

- `gateway/gateway_server/gateway_server.cpp`
- `gateway/gateway_server/gateway_server.hpp`

修复前：

```text
WSS frame
-> parse
-> msg_processor_->process_message(...)
-> future
-> std::thread(...).detach() 等待 future
-> send response
```

这个模型在高频消息下有两个问题：

- `process_message()` 内部已经创建异步任务，Gateway 外层又额外创建等待线程。
- 消息越多，线程创建、销毁、上下文切换越多，过载时没有明确拒绝策略。

修复后：

```text
WSS frame
-> parse
-> inflight 计数检查
-> ThreadPool::Enqueue(one business task)
-> process_message_sync(...)
-> send response / close session when needed
```

每条正常 WSS 消息现在只投递一个业务任务，不再创建每消息 detached 等待线程。

### 3. 初步流控

新增配置项：

```json
{
  "gateway": {
    "max_ws_inflight_messages": 4096
  }
}
```

影响文件：

- `config/dev.json`
- `config/benchmark.json`
- `config/dev.remote-all.json`
- `config/dev.remote-push.json`

Gateway 启动时读取 `gateway.max_ws_inflight_messages`，默认值为 `4096`。WSS 消息进入业务线程池前会增加 inflight 计数；任务结束后通过 RAII guard 递减。超过上限时立即返回 protobuf 错误响应：

```text
Gateway is busy, please retry later.
```

当前这是 inflight cap，不是最终形态的独立 bounded queue。它覆盖“排队中 + 执行中”的消息数量，先避免无限堆积。

### 4. 认证失败与超时关闭

修复前：

```text
std::thread([session] {
  sleep 100ms;
  session->close();
}).detach();
```

修复后：

```text
GatewayServer::schedule_delayed_close(...)
-> ThreadPool::Enqueue(...)
```

覆盖场景：

- WSS 消息处理返回 `AUTH_FAILED` 后延迟关闭。
- WebSocket 连接阶段 token 无效后延迟关闭。
- 未认证连接 30 秒超时后延迟关闭。

这样异常流量也不会继续制造 detached 线程。

### 5. Token 校验职责调整

通用 `MessageProcessor` 现在只对 HTTP 消息做统一 token 校验。WebSocket 消息由具体 handler 负责校验身份：

```text
MessageWsHandler::handle_send
-> verify_access_token(header.token(), token_user)
-> token_user.user_id 作为真实 sender
```

原因：

- WS 发送路径原先在 `MessageProcessor` 和 `MessageWsHandler` 中重复验签。
- `MessageWsHandler` 需要从 token 中提取真实 sender，因此 handler 层校验不能删。
- 保留 handler 校验并跳过通用 WS 校验，可以减少热路径重复开销，同时不信任客户端传入的 `from_uid`。

## Observability

`GatewayServer::get_server_stats()` 增加：

```text
ws.inflight_messages
ws.max_inflight_messages
thread_pool.threads
thread_pool.queued_or_running_tasks
```

后续压测时可以先观察 inflight 与线程池任务数是否持续增长。

## Verification

已验证：

```bash
cmake --build build/remote-push-odb --target gateway_server -j2
ctest --test-dir build/remote-push-odb -R GatewayMessageWsTest --output-on-failure
```

结果：

```text
gateway_server built successfully
GatewayMessageWsTest passed
```

注意：第一次运行 `GatewayMessageWsTest` 时 Redis 未启动，测试失败并触发进程退出异常。通过以下命令启动依赖后重跑通过：

```bash
docker compose up -d redis postgres
```

## Remaining Work

本轮只完成 Gateway WSS 调度模型的第一阶段收口。后续仍建议继续：

- 抽象 Gateway 专用业务 executor，不直接依赖全局 `ThreadPool`。
- 将 inflight cap 演进为真正 bounded queue，记录 reject/drop 计数。
- 增加 WS 单条消息处理耗时统计和 p50/p95/p99。
- 拆分 ack 与 push fanout，消息持久化成功后优先 ack。
- 将 delivered 标记从发送主路径迁移到 push worker。
- 优化消息持久化路径，减少插入后的重复读取和 delivered 的 load + update。

## Interview Notes

这次修复可以作为面试中的性能优化案例：

- 先通过压测定位现象：RTT 秒级、错误数上升、入口和调度模型不稳定。
- 再从调用链定位根因：每条消息 `std::async` + Gateway detached thread。
- 修复方向不是盲目加机器，而是把并发模型变成可控：固定线程池、inflight 上限、过载快速失败、统计暴露。
- 取舍上先实现 inflight cap，保留后续 bounded queue 和专用 executor 作为演进方向。
