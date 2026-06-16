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
