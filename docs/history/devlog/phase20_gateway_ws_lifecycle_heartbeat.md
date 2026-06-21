# Phase 20: Gateway WebSocket Lifecycle And Heartbeat

Date: 2026-06-21

## Background

2026-06-21 完整压测后，Gateway 进程已经不再直接崩溃，但 WSS 连接状态仍然异常：

- 修复前完整压测后残留 `98 ESTAB + 21 CLOSE-WAIT`。
- 第一轮 close 清理修复后，`CLOSE-WAIT` 已回落为 `0`，但仍观察到约 `26 ESTAB` 在压测结束 60 秒后残留。

结论是问题被拆成两层：

1. `CLOSE-WAIT` 主要来自异常关闭路径没有统一清理 socket/session。
2. `ESTAB` 残留说明仍缺少 idle timeout/heartbeat 机制，异常客户端或半开连接可能长期占用 fd 和 session。

## Fixed Scope

### 1. WebSocketSession 统一关闭路径

修改文件：

- `common/network/websocket_session.cpp`
- `common/network/websocket_session.hpp`

修复前：

```text
read/write/handshake error
-> defaultErrorHandler(...)
-> 只对少量错误 remove_session
-> 其他错误只 log
```

修复后：

```text
read/write/handshake/send overflow/close
-> fail_and_close(...)
-> perform_close(...)
-> 幂等清理 send queue/buffer
-> graceful close 尝试
-> 强制 ssl shutdown + socket cancel/shutdown/close
-> remove_session + close_callback exactly-once
```

新增状态：

- `closed_`：保证关闭流程只执行一次。
- `registered_`：保证只对已经加入 `WebSocketServer` 的 session 执行 remove。

### 2. WebSocket idle timeout 和 keepalive ping

在 WebSocket 握手前设置 Beast timeout：

```text
handshake_timeout = 15s
idle_timeout = 30s
keep_alive_pings = true
```

语义：

- 握手阶段不能无限挂起。
- 空闲连接由 Beast 主动 ping/timeout 管理。
- 客户端不响应或连接半开时，读路径会收到错误并进入统一关闭清理。

同时增加 control frame debug 日志，观察 ping/pong/close。日志 callback 使用 `weak_ptr` 捕获，避免 `session -> ws_stream -> callback -> session` 引用环。

### 3. WebSocketServer::stop 无锁关闭

修改文件：

- `common/network/websocket_server.cpp`

修复前：

```text
lock sessions_mutex_
-> 遍历 sessions_
-> session->close()
```

风险：

- `session->close()` 后续可能进入 `remove_session()` 再抢 `sessions_mutex_`。

修复后：

```text
lock sessions_mutex_
-> copy SessionPtr list
-> clear sessions_
unlock
-> 逐个 session->close()
```

### 4. CMD_HEARTBEAT 业务心跳

修改文件：

- `gateway/gateway_server/gateway_server.cpp`
- `gateway/gateway_server/gateway_server.hpp`
- `test/gateway_message/test_gateway_message_ws.cpp`
- `test/gateway/gateway_websocket_test.cpp`

Gateway 注册 `CMD_HEARTBEAT` handler：

```text
CMD_HEARTBEAT
-> verify access token
-> 校验 header.device_id 与 token.device_id
-> 返回 protobuf BaseResponse
```

成功响应：

```text
IMHeader.cmd_id = CMD_HEARTBEAT
IMHeader.seq = request.seq
BaseResponse.error_code = SUCCESS
```

失败响应：

```text
BaseResponse.error_code = AUTH_FAILED
error_message = Invalid or expired access token / Device ID mismatch
```

## Verification

依赖：

```bash
docker compose up -d redis postgres
```

构建：

```bash
cmake --build build/remote-push-odb --target gateway_server test_gateway_message_ws -j2
```

结果：

```text
gateway_server built successfully
test_gateway_message_ws built successfully
```

测试：

```bash
ctest --test-dir build/remote-push-odb -R GatewayMessageWsTest --output-on-failure
./build/remote-push-odb/test/gateway_message/test_gateway_message_ws --gtest_filter='GatewayMessageWsTest.Heartbeat*'
```

结果：

```text
GatewayMessageWsTest passed
GatewayMessageWsTest.HeartbeatWithValidTokenReturnsSuccess passed
GatewayMessageWsTest.HeartbeatDeviceMismatchReturnsAuthFailed passed
```

注意：

- `test/gateway/gateway_websocket_test.cpp` 也补了真实 TLS WebSocket 心跳端到端用例。
- 当前 `build/remote-push-odb` 默认未启用 `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`，所以该 legacy 测试不作为默认验收项。
- 尝试开启 legacy gateway tests 时，`test/gateway/CMakeLists.txt` 对 `redis++` 的 `find_package` 在当前 vcpkg 配置下失败，后续如要恢复该测试集，需要先清理 legacy 测试依赖。

## Remaining Work

### 远端复测结果

已部署最新 `gateway_server` 到压测服务器并重跑 WSS 连接场景：

```bash
python3 test/benchmark/run_all.py --wss-only --phases conn
```

复测报告：

- `docs/benchmark/benchmark_report_20260621_ws_lifecycle_retest/benchmark_report_20260621_ws_lifecycle_retest.md`

连接结果：

```text
conn-10:  10/10
conn-50:  50/50
conn-100: 100/100
conn-200: 125/200
```

压测结束并等待 idle timeout 后，SUT 状态：

```text
ss -tan "( sport = :10001 )" | awk 'NR>1{print $1}' | sort | uniq -c
-> 1 LISTEN
```

验收判断：

- `CLOSE-WAIT=0`：通过。
- 客户端退出且超过 idle timeout 后，`ESTAB=0`：通过。
- Gateway 进程存活：通过。
- `conn-200` 成功率仍不足：转入 WSS 建连性能专项。

### 后续优化

- `GatewayServer::schedule_delayed_close()` 仍使用全局线程池 `sleep_for` 做延迟关闭，后续应迁移到 Asio timer 或 session 内部 timer。
- 当前 Beast idle timeout 是硬编码常量，后续可迁移到 `config/dev.json` / `config/benchmark.json`。
- WSS 建连路径需要补观测：SSL handshake 耗时、HTTP upgrade read 耗时、WebSocket accept 耗时、active handshakes。
- Gateway legacy integration tests 需要清理 `redis++` 查找和测试依赖后再纳入默认构建。
