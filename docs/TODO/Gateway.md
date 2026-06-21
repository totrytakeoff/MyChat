1. 服务 http请求 url 应该基于配置文件! 而不是硬编码在code当中! (gateway_server 中的 register_xxx_on_server)
2. 优化其中初始化逻辑职责(这写的什么玩意儿,一堆的宏if包着的各个模块的初始化逻辑全都往init_server中塞,也不拆分成多个函数封装)

## 2026-06-21 Gateway WSS 连接生命周期问题

压测报告:

- `docs/benchmark/benchmark_report_20260621_full/benchmark_report_20260621_full.md`

现象:

- 完整 WSS + HTTP 阶梯压测后 gateway 未崩溃，但服务端 WSS 仍残留 `98 ESTAB + 21 CLOSE-WAIT`。
- `conn-50` 基本稳定，`conn-100` 开始超时，`conn-200` 成功率降至 16%。
- 已建立连接的消息 RTT 仍在几十毫秒级，说明当前主瓶颈不是消息处理慢，而是连接建立和连接释放链路不稳定。

已定位并修复的代码风险点:

- [x] `common/network/websocket_session.cpp::defaultErrorHandler` 只对 `websocket::error::closed`、`net::error::eof`、`net::error::connection_reset` 做 `remove_session`，对 `stream truncated`、`operation_aborted`、TLS/Beast 读写异常等错误只打印日志，未关闭底层 socket，未移除 session。
- [x] `common/network/websocket_session.cpp::close` 在 `ws_stream_.close()` 返回错误时直接 `return`，导致 `server_->remove_session()` 和 `close_callback_` 不执行。
- [x] `common/network/websocket_session` 缺少幂等关闭标记，多个 read/write/timeout/业务关闭路径可能重复进入关闭逻辑。
- [x] `common/network/websocket_session` 缺少 WSS idle/read timeout，异常客户端或半开连接可能长期占用 session/fd。
- [x] `common/network/websocket_server.cpp::stop` 持有 `sessions_mutex_` 时调用 `session->close()`，而 close 后续可能再次进入 `remove_session()` 获取同一把锁，存在潜在死锁风险。
- [x] Gateway 缺少独立 `CMD_HEARTBEAT` 业务心跳 handler，客户端无法用统一协议主动保活和探测 token/device 状态。
- [ ] `gateway/gateway_server.cpp::schedule_delayed_close` 使用全局业务线程池 `sleep_for` 实现延迟关闭，高压下可能占用业务 worker；后续应迁移到 Asio timer 或 session 内部定时器。

当前状态:

- 已实现统一 close 清理、幂等关闭、强制 socket shutdown、stop 无锁关闭、Beast idle timeout/keepalive ping。
- 已实现 `CMD_HEARTBEAT` protobuf 业务回包，校验 access token 与 device_id。
- 本地验证通过：`gateway_server` 构建、`GatewayMessageWsTest`、`GatewayMessageWsTest.Heartbeat*`。
- 远端复测通过：压测结束并等待 idle timeout 后，WSS 端口只剩 `LISTEN`，`CLOSE-WAIT=0`，`ESTAB=0`。
- 剩余问题：`conn-200` 成功率为 `125/200`，说明 WSS 建连能力仍需单独优化。

复测报告:

- `docs/benchmark/benchmark_report_20260621_ws_lifecycle_retest/benchmark_report_20260621_ws_lifecycle_retest.md`

## 2026-06-21 Gateway WSS 建连观测

压测报告:

- `docs/benchmark/benchmark_report_20260621_ws_observe/benchmark_report_20260621_ws_observe.md`

已补齐的观测能力:

- [x] `WebSocketServer` 统计 accept 成功/失败、active handshakes、当前 session 数。
- [x] `WebSocketSession` 统计 SSL handshake、HTTP upgrade read、WS accept、session add 阶段耗时。
- [x] `GatewayServer::get_server_stats()` 输出 WSS 建连阶段统计。
- [x] `GET /api/v1/stats` 暴露只读运行状态，便于压测期间远程抓取。

本轮定位结论:

- PVE 公网发压 `150/200 users` 稳定只有 `125` 个连接进入 Gateway 应用层，失败均为客户端 `connect_timeout`。
- Gateway stats 显示进入应用层的连接全部能完成 TLS/WS/session_add，且 session add 平均低于 1ms。
- SUT 本机 loopback 压 `150 users @ 20/s` 全成功，connect avg `2.80ms`。
- 因此当前不应优先改消息处理、DB、push fanout；下一步应先查公网 TCP 建连路径、云侧安全策略和 SUT TCP 参数。

后续 Gateway 侧可优化项:

- [ ] 如果网络路径排除后仍有 Gateway accept 瓶颈，再拆分 acceptor io_context 与 session handshake io_context。
- [ ] `WebSocketServer::do_accept()` 在 accept 出错后应区分关闭态和可恢复错误，可恢复错误继续 accept。
- [ ] WSS 建连统计从 count/avg/max 扩展为分位数。
- [ ] `/api/v1/stats` 后续可改为 JSON，便于 benchmark 脚本自动解析。
