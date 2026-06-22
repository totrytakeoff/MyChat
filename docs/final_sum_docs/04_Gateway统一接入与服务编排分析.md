# Gateway 统一接入与服务编排分析

日期：2026-06-16

## 1. 这篇文档先回答什么

MyChat 的第一入口不是某个 service，而是 Gateway。

如果只看业务名词，很容易把项目理解成“User、Message、Friend、Group、Push 一堆服务拼起来”；但真正的调用路径应该先从 Gateway 读起，因为它负责：

- HTTP 和 WebSocket 的统一接入。
- Access Token 校验和当前用户身份提取。
- local/remote facade 的选择。
- Push 在线投递和 Gateway callback。
- 把客户端请求转成服务层调用。

一句话概括：

```text
Gateway 是 MyChat 的协议边界和服务编排层。
```

## 2. 先看哪些代码

建议按这个顺序读：

1. `gateway/main.cpp`
2. `gateway/gateway_server/gateway_server.cpp`
3. `gateway/README.md`
4. `gateway/http/user_http_controller.cpp`
5. `gateway/http/message_http_controller.cpp`
6. `gateway/http/friend_http_controller.cpp`
7. `gateway/http/group_http_controller.cpp`
8. `gateway/http/remote_*_client.cpp`
9. `gateway/http/*_client.cpp`
10. `gateway/push/remote_push_notifier.cpp`
11. `gateway/push/gateway_push_delivery_service.cpp`
12. `services/push/push_server_app.cpp`

## 3. 启动入口

`gateway/main.cpp` 做的是进程级启动：

- 解析命令行参数。
- 读取 config。
- 初始化 Redis。
- 构造 `GatewayServer`。
- 注册优雅退出。
- 启动 WebSocket 和 HTTP 服务。

这里最关键的不是参数多，而是它把 Gateway 作为一个完整进程启动，而不是“某个业务模块的附属代码”。

## 4. GatewayServer 初始化顺序

`gateway/gateway_server/gateway_server.cpp` 是真正的编排中心。

初始化顺序大致是：

```text
logger
-> thread pool
-> IO service pool
-> message parser / processor
-> ODB database
-> service facade 选择
-> HTTP / WS server
-> connection manager
-> push notifier / delivery service
-> message handlers
```

这里有两个面试重点：

- 依赖顺序是固定的，不能乱调。
- 不是所有逻辑都在 Gateway 里实现，很多只是 facade 选择和请求编排。

## 5. HTTP 入口怎么进来

Gateway 的 HTTP 路由不是散落在控制器里，而是先在 `GatewayServer` 里注册，再转进 controller：

- `/api/v1/auth/register`
- `/api/v1/auth/login`
- `/api/v1/auth/info`
- `/api/v1/auth/profile`
- `/api/v1/users/search`
- `/api/v1/messages/*`
- `/api/v1/friends/*`
- `/api/v1/groups/*`
- `/api/v1/groups/messages/*`

以 `UserHttpController` 为例：

```text
HTTP request
-> UserHttpController
-> UserClient
-> LocalUserClient or RemoteUserClient
-> UserService or gRPC user_server
```

controller 只负责：

- 解析请求体。
- 读 Authorization header。
- 生成 token。
- 映射业务错误为 HTTP 状态码。

它不直接碰持久化细节。

## 6. local / remote facade 怎么切

Gateway 不直接依赖“某个服务是本地还是远程”，而是先选择 facade：

- `LocalUserClient`
- `RemoteUserClient`
- `LocalMessageClient`
- `RemoteMessageClient`
- `LocalFriendClient`
- `RemoteFriendClient`
- `LocalGroupClient`
- `RemoteGroupClient`
- `PushService`
- `RemotePushNotifier`

选择逻辑来自 config：

- `user.mode`
- `message.mode`
- `friend.mode`
- `group.mode`
- `push.mode`

核心行为：

- `local` 时直接调用进程内 service。
- `remote` 时走 gRPC client。
- 如果编译时没打开对应 remote client，即使配置写了 remote，也会回退到 local，并打 warning。

这就是本项目的兼容策略：

```text
配置决定意图，编译开关决定能力，Gateway 负责兜底。
```

## 7. WebSocket 入口怎么进来

WebSocket 处理在 `gateway_server.cpp` 中绑定：

- 先解析 protobuf/协议包。
- 再交给 message processor。
- 再由 `MessageWsHandler` 做发送、ack、鉴权和错误处理。
- 成功后可能触发 push。

这条链路里最重要的是：

- WebSocket 不只是“消息通道”，它也是在线态入口。
- 消息推送是否成功，和消息是否持久化是两件事。
- 失败后仍应保留离线拉取能力。

### 7.1 WebSocket 调度与背压修复

压测后对 WSS 链路做过一次关键收口。修复前的路径是：

```text
WSS frame
-> MessageProcessor::process_message
-> std::async(std::launch::async)
-> Gateway 再创建 std::thread(...).detach() 等待 future
-> 发送响应
```

这个设计在低频场景下能工作，但高频消息下会带来大量线程创建、销毁和上下文切换。更严重的是，过载时没有明确的并发上限和拒绝策略，消息会持续堆积，最后表现为 RTT 秒级、错误数上升。

当前修复后的路径是：

```text
WSS frame
-> parse protobuf envelope
-> Gateway inflight limit check
-> ThreadPool::Enqueue(one business task)
-> MessageProcessor::process_message_sync
-> MessageWsHandler::handle_send
-> protobuf ack / protobuf error response
```

对应代码点：

- `gateway/gateway_server/gateway_server.cpp`：WSS 回调、`ws_inflight_messages_`、`max_ws_inflight_messages_`、`schedule_delayed_close()`。
- `gateway/message_processor/message_processor.cpp`：`process_message()` 投递线程池，`process_message_sync()` 执行同步核心。
- `config/dev.json` / `config/benchmark.json`：`gateway.max_ws_inflight_messages`。

这里的设计取舍是：

- 先把不可控的“每消息动态线程”改成固定线程池调度。
- 先用 inflight cap 兜住排队中和执行中的消息数量。
- 过载时快速返回 overload，而不是继续把请求堆到系统资源耗尽。
- WebSocket token 校验放在具体 handler 中完成，因为 handler 需要从 token 中提取真实 sender；通用 processor 不再重复验签。

这不是最终版性能架构。后续还应该把全局 `ThreadPool` 演进为 Gateway 专用业务 executor，并补充真正 bounded queue、拒绝计数和处理耗时分位数。但作为第一阶段修复，它已经把 WSS 主链路从不可控并发模型拉回了可控并发模型。

面试里可以这样讲：

```text
我在压测后发现 WSS 高负载下 RTT 会进入秒级，不是因为某个算法特别慢，而是 Gateway 的调度模型有问题：每条消息先 std::async，再额外创建 detached thread 等待 future。后来我把这条链路收敛为固定线程池执行，并加入 max_ws_inflight_messages 做初步背压，超过上限直接返回 overload。这样系统在过载时会可控退化，而不是无限创建线程和堆积任务。
```

### 7.2 当前 Gateway 压测基线

最新可引用压测报告：

```text
docs/benchmark/benchmark_report_20260622_full_retest/benchmark_report_20260622_full_retest.md
```

| 场景 | 结果 | 说明 |
| --- | ---: | --- |
| HTTP ramp | 500 VU，46230 请求，失败率 0%，p95 5.81ms | HTTP 接入层修复后稳定 |
| WSS 建连 | 200 连接，失败 0，connect p95 约 15ms | 200 连接规模内建连稳定 |
| WSS 消息 | 200 用户 / 1s，RTT p95 12.60ms | 常规实时聊天压力稳定 |
| WSS 消息 | 200 用户 / 500ms，RTT p95 21.92ms | 高频聊天仍保持低延迟 |
| WSS 极限 | 200 用户 / 250ms，RTT p95 2628.05ms | 进入当前吞吐拐点 |

这组数据的解读要克制：可以说当前 MVP 在 200 在线用户、0.5-1s 发一条消息的场景下实时链路体感流畅，也可以说 HTTP keep-alive worker 占用问题已经收敛；但不能说已经达到商业 IM 或百万连接能力。250ms 间隔极限场景暴露的不是建连或解码失败，而是消息处理、持久化、push 或写队列中的某个环节开始排队。

## 8. Push 的双向链路

远程 Push 是 Gateway 里最能体现分布式边界的一段。

正常链路：

```text
Gateway
-> RemotePushNotifier
-> push_server NotifyUser
```

但 PushServer 不能直接拿 Gateway 内存里的 WebSocket session，所以又需要反向 callback：

```text
push_server
-> GatewayPushDeliveryService
-> session lookup / payload send / delivered mark
```

也就是说，远程 Push 不是单向 RPC，而是双向协作。

这段设计的本质原因是：

- session 的归属在 Gateway。
- Push 服务只负责投递调度。
- 真正的连接操作必须回到 Gateway。

## 9. local / remote 运行模式

当前项目有三种常见运行形态：

- 默认 local：所有服务在进程内调用，最适合开发和调试。
- remote push：只有 Push 拆出去，验证 Gateway callback。
- remote-all：User / Message / Friend / Group / Push 全部拆成独立进程。

对应配置：

- `config/dev.json`
- `config/dev.remote-push.json`
- `config/dev.remote-all.json`

对应脚本：

- `scripts/dev/run_remote_push_stack.sh`
- `scripts/dev/run_remote_services_stack.sh`

## 10. 面试时怎么讲

可以这样说：

```text
我把 Gateway 作为统一接入层和服务编排层，HTTP 和 WebSocket 都先进入 Gateway。Gateway 不直接写死业务逻辑，而是通过 local/remote facade 把请求转到进程内 service 或远程 gRPC service。这样既保留了本地开发效率，也保留了分布式服务边界。Push 是最典型的一段，因为 WebSocket session 在 Gateway，所以远程 Push 不能直接操作连接，必须通过 Gateway callback 完成 session lookup 和 payload 投递。
```

## 11. 这篇之后怎么读

读完这篇，再往后应该按下面顺序：

1. `05_注册登录与鉴权链路分析.md`
2. `06_单聊消息链路分析.md`
3. `07_好友关系链路分析.md`
4. `08_群聊链路分析.md`
5. `09_实时推送与Push服务分析.md`
6. `10_gRPC服务边界与远程调用分析.md`
7. `11_数据持久化与缓存设计分析.md`
