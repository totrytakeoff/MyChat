# Gateway 统一接入与服务编排分析

日期：2026-06-23

## 1. 这篇文档回答什么

MyChat 的第一入口是 Gateway。理解项目时必须先从 Gateway 入手，再进入 User、Message、Friend、Group、Push 等服务。

当前 Gateway 的核心定位是：

```text
协议入口 + 鉴权边界 + 连接管理 + cmd 路由 + packet 转发
```

它不再是旧文档中描述的“HTTP controller 集合”，也不再通过 `Local*Client / Remote*Client facade` 作为主链路。

当前主链路是：

```text
HTTP / WebSocket
-> MessageParser
-> UnifiedMessage
-> MessageProcessor
-> GatewayCommandHandlerRegistry
-> GatewayRuntimeRegistry
-> local service packet dispatcher 或 remote ForwardPacket gRPC
```

## 2. 先看哪些代码

建议按这个顺序读：

1. `gateway/main.cpp`
2. `gateway/gateway_server/gateway_server.cpp`
3. `gateway/message_processor/message_parser.cpp`
4. `gateway/message_processor/message_processor.cpp`
5. `gateway/command_handlers/gateway_command_handler_registry.cpp`
6. `gateway/command_handlers/gateway_command_handler_registry.hpp`
7. `gateway/command_handlers/user_command_handlers.cpp`
8. `gateway/command_handlers/message_command_handlers.cpp`
9. `gateway/command_handlers/friend_command_handlers.cpp`
10. `gateway/command_handlers/group_command_handlers.cpp`
11. `gateway/ws/message_ws_handler.cpp`
12. `gateway/service_adapters/remote_*_service_adapter.cpp`
13. `services/*/*_packet_dispatcher.cpp`

阅读原则：

- 先看 Gateway 如何把 HTTP/WS 统一成 `UnifiedMessage`。
- 再看 handler 如何只做鉴权、补 header、转发 packet。
- 最后看各 service packet dispatcher 如何解析 payload 并处理业务。

## 3. Gateway 启动入口

`gateway/main.cpp` 负责进程级启动：

- 解析命令行参数。
- 读取 config。
- 设置最大打开文件数。
- 初始化 Redis。
- 构造 `GatewayServer`。
- 注册信号处理和优雅退出。
- 启动 WebSocket 和 HTTP 服务。

当前有一个待修工程问题：

- 压测中发现 `gateway_server -w/-H` 端口可能被配置文件 `gateway.websocket_port/http_port` 覆盖。
- 后续应明确为命令行参数优先级高于配置文件，避免部署时误判监听端口。

## 4. GatewayServer 初始化顺序

`gateway/gateway_server/gateway_server.cpp` 是 Gateway 的运行时编排中心。

当前初始化大致顺序：

```text
init_logger
-> init_io_service_pool
-> ThreadPool::Init
-> init_msg_parser
-> init_msg_processor
-> init_database_runtime
-> init_user_runtime
-> init_message_runtime
-> init_friend_runtime
-> init_group_runtime
-> init_group_message_runtime
-> init_ws_server
-> init_http_server
-> init_conn_mgr
-> init_message_ws_runtime
-> register_message_handlers
```

关键点：

- service runtime 先初始化，再注册 command handlers。
- `GatewayRuntimeRegistry` 持有 user/message/friend/group 的 local service 或 remote adapter。
- `register_gateway_command_handlers()` 根据 runtime registry 注册 user/message/friend/group 命令。
- HTTP 和 WebSocket 最终都进入同一套 `MessageProcessor` 命令分发模型。

## 5. HTTP 请求链路

HTTP 入口当前不是每个业务模块单独注册 controller route，而是：

```text
httplib catch-all
-> MessageParser::parse_http_request_enhanced(req)
-> RouterManager::parse_http_route(method, path)
-> UnifiedMessage
-> MessageProcessor::process_message()
-> MessageProcessor::process_message_sync()
-> command handler
-> service packet dispatcher 或 remote ForwardPacket
-> ProcessorResult::http(...)
-> httplib::Response
```

代码点：

- `GatewayServer::init_http_server()`：注册 `/api/v1/health`、`/api/v1/stats` 和 catch-all。
- `MessageParser::parse_http_request_enhanced()`：解析 method/path/token/device/platform/body/query。
- `RouterManager::parse_http_route()`：根据配置得到 `cmd_id`。
- `MessageProcessor::process_message()`：投递线程池。
- `MessageProcessor::process_message_sync()`：按 `cmd_id` 找 handler。

HTTP 业务路由来自配置，不再来自旧的 `register_*_http_routes()`：

- `config/dev.json`
- `config/dev.remote-push.json`
- `config/dev.remote-all.json`
- `config/benchmark.json`

## 6. WebSocket 请求链路

WebSocket 入口当前链路：

```text
WebSocket frame
-> ProtobufCodec::decodeEnvelope
-> MessageParser::parse_websocket_message_enhanced
-> UnifiedMessage
-> Gateway WSS inflight limit
-> ThreadPool::Enqueue(one business task)
-> MessageProcessor::process_message_sync
-> CMD_SEND_MESSAGE handler
-> MessageWsHandler::handle_send
-> message service packet dispatcher 或 remote ForwardPacket
-> protobuf envelope ack/error
```

代码点：

- `GatewayServer::init_ws_server()`：绑定 WSS frame callback。
- `gateway/ws/message_ws_handler.cpp`：处理 `CMD_SEND_MESSAGE` 的 WS 发送。
- `common/network/packet_error_builder.*`：构造 Gateway 本地拦截错误的 packet response。

WSS 当前已经补齐：

- TLS/WS handshake 观测。
- session add/remove 观测。
- idle timeout。
- keepalive ping。
- `CMD_HEARTBEAT`。
- `max_ws_inflight_messages` 初步背压。
- 压测结束后 session 清理。

## 7. MessageProcessor 的职责

`MessageProcessor` 当前非常关键，但职责很窄：

- 保存 `cmd_id -> handler` 映射。
- 将消息处理投递到全局线程池。
- 同步执行对应 handler。
- 处理无 handler、空消息、异常等通用错误。

它不再做统一强制 token 校验。

原因：

- 注册/登录是公开入口，不能在通用层强制验签。
- HTTP 和 WS 的鉴权语义不同。
- handler 需要按命令语义决定是否鉴权，并把 token uid 写入 service packet header。

所以当前鉴权位置在 command handler / WS handler 中。

## 8. GatewayCommandHandlerRegistry

`GatewayCommandHandlerRegistry` 负责集中注册业务命令：

```text
register_gateway_command_handlers
-> register_user_command_handlers
-> register_message_command_handlers
-> register_friend_command_handlers
-> register_group_command_handlers
```

核心上下文：

```cpp
struct GatewayCommandHandlerContext {
    const GatewayRuntimeRegistry* runtime = nullptr;
};
```

`GatewayRuntimeRegistry` 中保存：

- local `UserService`
- remote `UserServiceAdapter`
- local `MessageService`
- remote `MessageServiceAdapter`
- local `FriendService`
- remote `FriendServiceAdapter`
- local `GroupService`
- local `GroupMessageService`
- remote `GroupServiceAdapter`
- `MultiPlatformAuthManager`
- `PushNotifier`
- `MessageWsHandler`

这比旧的展开式 context 更收敛：handler 不再持有一堆散落成员，而是统一从 runtime registry 取依赖。

## 9. Command Handler 的边界

handler 可以做：

- 检查 service/adapter 是否存在。
- 校验 HTTP token 或 WS token。
- 从 token 中提取可信 `uid/device/platform`。
- 把可信身份写入 packet header。
- 构造 `XxxPacketRequest`。
- 调用 local packet dispatcher 或 remote adapter。
- 根据 service 返回的 `http_status/payload/base` 构造 HTTP/WS 响应。
- 消费 `auth_token_hint` 生成 token。
- 消费 `push_event` 转交 PushNotifier。

handler 不应该做：

- 解析注册字段、好友字段、群字段、消息正文等业务 payload。
- 构造业务 DTO。
- 查询业务 repository。
- 实现好友、群成员、消息状态等业务规则。
- 自己计算群 fanout 成员。
- 维护业务 response mapper。

一句话：

```text
Gateway handler 是 packet 转发边界，不是业务 service。
```

## 10. User 命令链路

HTTP user 请求：

```text
HTTP /api/v1/auth/register 或 /login 等
-> MessageParser
-> CMD_REGISTER / CMD_LOGIN / CMD_GET_USER_INFO / CMD_UPDATE_USER_INFO / CMD_SEARCH_USER
-> user_command_handlers.cpp
-> UserPacketRequest
-> local UserPacketDispatcher 或 remote UserService::ForwardPacket
-> UserPacketResponse
-> Gateway 注入 token 或直接返回 payload
```

特殊点：

- 注册/登录不要求已有 access token。
- 资料查询、资料更新、用户搜索需要 access token。
- token 仍由 Gateway 签发，这是鉴权边界的一部分。
- User service 返回 `auth_token_hint`，Gateway 根据 hint 生成 access/refresh token 并注入响应。

## 11. Message 命令链路

HTTP message：

```text
HTTP /api/v1/messages/*
-> CMD_SEND_MESSAGE / CMD_MESSAGE_HISTORY / CMD_PULL_MESSAGE
-> message_command_handlers.cpp
-> MessagePacketRequest(application/json)
-> local MessagePacketDispatcher 或 remote MessageService::ForwardPacket
-> MessagePacketResponse
-> optional push_event
```

WebSocket message：

```text
WS CMD_SEND_MESSAGE
-> message_command_handlers.cpp
-> MessageWsHandler::handle_send
-> MessagePacketRequest(protobuf payload)
-> local MessagePacketDispatcher 或 remote MessageService::ForwardPacket
-> MessagePacketResponse
-> protobuf envelope ack
-> optional push_event
```

关键边界：

- Gateway 只校验 token 和 header 级 receiver。
- 发送消息 payload 的 protobuf 解析在 Message service packet dispatcher。
- delivered 状态更新不在 Gateway 中做。

## 12. Friend 命令链路

```text
HTTP /api/v1/friends/*
-> CMD_ADD_FRIEND / CMD_HANDLE_FRIEND_REQUEST / CMD_GET_FRIEND_LIST / CMD_GET_FRIEND_REQUESTS
-> friend_command_handlers.cpp
-> FriendPacketRequest(application/json)
-> local FriendPacketDispatcher 或 remote FriendService::ForwardPacket
-> FriendPacketResponse
```

好友业务规则全部在 Friend service 内：

- 是否能添加自己。
- 目标用户是否存在。
- 是否重复申请。
- 是否有权限处理申请。
- pending 状态是否可变更。

## 13. Group 命令链路

```text
HTTP /api/v1/groups/*
-> group_command_handlers.cpp
-> GroupPacketRequest(application/json)
-> local GroupPacketDispatcher 或 remote GroupService::ForwardPacket
-> GroupPacketResponse
-> optional push_events
```

Group service 负责：

- 群资料解析。
- 群成员校验。
- 群成员列表。
- 群消息保存。
- 群消息 fanout 成员计算。

Gateway 只把 `push_events` 转给 PushNotifier，不自己展开群成员。

## 14. Push 链路

Push 的输入来自 Message/Group service 返回的 `push_event` 或 `push_events`。

local push：

```text
Gateway command handler / MessageWsHandler
-> PushNotifier
-> PushRuntime
-> ConnectionManager
-> WebSocketServer::send
```

remote push：

```text
Gateway
-> RemotePushNotifier
-> push_server NotifyUser
-> PushRuntime
-> GatewayPushDeliveryService callback
-> Gateway session lookup / send payload
```

为什么需要 callback：

```text
WebSocket session 属于 Gateway 进程内存。
远程 push_server 不能直接访问 Gateway 的 session map。
所以 remote push 只能通过 Gateway 暴露的 delivery callback 回到 Gateway 投递。
```

这是项目里很值得讲的分布式边界设计。

## 15. Local / Remote 怎么切

当前不要再用旧说法“local/remote facade”作为主叙事。更准确是：

```text
GatewayRuntimeRegistry
-> local service instance
或
-> remote service adapter
```

remote adapter 做的事情也很窄：

- 接收 `XxxPacketRequest`。
- 调用远程 gRPC `ForwardPacket`。
- 返回 `XxxPacketResponse`。
- 做 deadline/status/base error 的传输适配。

remote adapter 不应该重新暴露业务方法，比如 `send_text_message`、`create_group`、`send_request` 这类旧接口已经删除。

## 16. Gateway 当前观测能力

`GET /api/v1/stats` 当前输出：

- WSS accept 成功/失败。
- 当前 session 数。
- TLS/HTTP upgrade/WS accept/session add 耗时。
- HTTP worker、queue、keep-alive 配置。
- HTTP 请求数、inflight、状态码统计。
- HTTP route 级 count/avg/max。
- parser decode/routing 失败数。
- WSS inflight 消息数。
- 线程池线程数、排队/运行任务数。
- service runtime mode/local/remote bound 状态。

这让压测时可以把“客户端看到 timeout”和“Gateway 内部是否有堆积”对应起来。

## 17. 重构后压测基线

最新完整压测：

```text
docs/benchmark/benchmark_report_20260623_gateway_refactor_full/benchmark_report_20260623_gateway_refactor_full.md
docs/benchmark/benchmark_report_20260623_gateway_refactor_full/performance_before_after_comparison.md
```

核心结果：

| 指标 | 结果 |
| --- | ---: |
| WSS `conn-200` | 200/200 |
| WSS 最重消息场景 | `200 users / 250ms` |
| WSS 吞吐 | `714.4 msg/s` |
| WSS RTT p95 | `35.91ms` |
| HTTP ramp 请求数 | `46296` |
| HTTP 平均 RPS | `306.86/s` |
| HTTP 失败率 | `0.00%` |
| HTTP p95 | `3.75ms` |

对比重构前 HTTP ramp：

| 指标 | 重构前 | 重构后 |
| --- | ---: | ---: |
| 平均 RPS | 14.44/s | 306.86/s |
| 失败率 | 56.03% | 0.00% |
| p95 | 59.99s | 3.75ms |

这里的正确表达是：

```text
重构没有引入性能退化，并且在统一入口、HTTP 接入修复、WSS 生命周期修复后，
当前 Gateway MVP 已有一组稳定压测基准。
```

不要夸大为“生产级百万连接”。

## 18. 面试时怎么讲

可以这样说：

```text
MyChat 的 Gateway 不是传统意义上的一堆 HTTP controller。
我把 HTTP 和 WebSocket 都统一成 UnifiedMessage，然后通过 MessageProcessor 按 cmd_id 分发。
Gateway handler 只做鉴权、身份注入、packet 转发和 push event 转交，
真正的业务 payload 解析在各 service 的 packet dispatcher 中完成。
这样 Gateway 的职责很清楚：它是协议入口和服务路由层，不是业务处理层。
同时，GatewayRuntimeRegistry 让同一套 handler 可以选择 local service 或 remote ForwardPacket gRPC，
所以项目可以从 all-in-one 开发模式平滑切到多进程分布式服务模式。
```

如果被追问“为什么要重构掉 controller/client”：

```text
因为原来的 controller/client 链路让 HTTP 和 WS 走了两套模型，
而且 Gateway 中出现了业务字段解析、业务 DTO 构造和 response mapper。
这违背了我最初对 Gateway 的设计：Gateway 只做协议入口、鉴权、路由和转发。
重构后 HTTP/WS 都回到 MessageParser + MessageProcessor + command handler registry，
业务解析下沉到 service packet dispatcher，Gateway 边界更干净，也更利于讲清分布式服务边界。
```

## 19. 当前仍需注意的问题

后续仍有改进项，但不阻塞当前 MVP：

- `XxxPacketRequest/Response` 仍和业务 proto 混放在同一个 proto 文件中，长期应拆出 `*_packet.proto` 或统一 `gateway_packet.proto`。
- `GatewayRuntimeRegistry` 仍是显式字段，未来可进一步收敛为泛化 endpoint map。
- Push/WS/Auth 的生命周期还可以继续纳入更统一的 runtime 管理。
- `/api/v1/stats` 目前是文本输出，后续应改为 JSON 便于 benchmark 自动解析。
- CLI/config 优先级需要修复，避免压测端口被配置覆盖。

## 20. 这篇之后怎么读

读完 Gateway 后，再按业务链路进入：

1. `05_注册登录与鉴权链路分析.md`
2. `06_单聊消息链路分析.md`
3. `07_好友关系链路分析.md`
4. `08_群聊链路分析.md`
5. `09_实时推送与Push服务分析.md`
6. `10_gRPC服务边界与远程调用分析.md`
7. `11_数据持久化与缓存设计分析.md`
