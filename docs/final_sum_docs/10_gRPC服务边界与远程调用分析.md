# MyChat gRPC 服务边界与远程调用分析

日期：2026-06-16

## 1. 文档目标

阅读这篇之前，建议先读 `04_Gateway统一接入与服务编排分析.md`。gRPC 边界不是孤立存在的，它是 Gateway facade 在 remote 模式下的另一条实现路径。

这份文档分析 MyChat 从本地服务调用演进到分布式 gRPC 边界的设计：

- 为什么不让 Gateway 直接写死所有业务逻辑。
- local/remote facade 是怎么工作的。
- User/Message/Friend/Group/Push 的 gRPC 边界分别解决什么问题。
- remote-all 本地多进程栈如何支撑分布式验证。
- 当前距离生产级服务治理还差什么。

一句话概括：

```text
MyChat 用 Gateway facade 把外部 HTTP/WebSocket 契约和内部服务部署方式解耦；
同一个 Gateway controller 可以调用本地 Service，也可以切到远程 gRPC stub，从而平滑演进到分布式服务。
```

## 2. 整体调用模型

统一模型：

```text
Web Client
-> Gateway HTTP Controller / WebSocket Handler
-> Gateway Client Facade
-> Local Service 或 Remote gRPC Client
-> Service Core
-> Repository / Runtime
-> PostgreSQL / Redis / WebSocket
```

核心点：

```text
Gateway 对外协议不变，内部服务部署方式可切换。
```

## 3. 为什么需要 facade

如果 Gateway controller 直接调用 gRPC stub，会带来：

- controller 充满 proto 转换逻辑。
- local 单进程调试困难。
- 单元测试要启动远程服务。
- gRPC 错误处理污染 HTTP controller。

facade 解决：

- controller 只面向领域接口。
- local client 调进程内 service。
- remote client 调 gRPC。
- 错误映射集中在 remote client。
- 测试边界更清晰。

示例：

```text
UserHttpController
-> UserClient
   -> LocalUserClient
   -> RemoteUserClient
```

## 4. 当前 gRPC 服务边界

### 4.1 User Service

proto：

```text
common/proto/user.proto
```

RPC：

```text
Register
Login
GetUserInfo
SearchUsers
UpdateUserInfo
```

职责：

- 账号注册。
- 登录凭证校验。
- 用户资料查询。
- 用户搜索。
- 资料更新。

### 4.2 Message Service

proto：

```text
common/proto/message.proto
```

RPC：

```text
SendMessage
GetConversation
PullOffline
MarkDelivered
MarkRead
```

职责：

- 单聊消息持久化。
- 历史查询。
- 离线拉取。
- 状态标记。

### 4.3 Friend Service

proto：

```text
common/proto/friend.proto
```

RPC：

```text
SendRequest
RespondToRequest
GetFriends
GetPendingRequests
```

职责：

- 好友申请。
- 申请处理。
- 好友列表。
- 待处理申请。

### 4.4 Group Service

proto：

```text
common/proto/group.proto
```

RPC：

```text
CreateGroup
JoinGroup
LeaveGroup
GetGroupInfo
SearchGroups
GroupExists
ListMyGroups
ListMembers
SendGroupMessage
GetGroupMessages
```

职责：

- 群资料。
- 群成员。
- 群消息。

### 4.5 Push Service

proto：

```text
common/proto/push.proto
```

RPC：

```text
PushService.NotifyUser
GatewayPushDeliveryService.ListUserSessions
GatewayPushDeliveryService.SendSessionPayload
GatewayPushDeliveryService.MarkMessageDelivered
```

职责：

- Push Service 接收投递请求。
- Gateway callback 负责实际 WebSocket session 操作。

## 5. local 模式

local 模式调用链：

```text
Gateway
-> LocalUserClient / LocalMessageClient / LocalFriendClient / LocalGroupClient / PushService
-> in-process Service
```

优点：

- 本地启动简单。
- 调试断点方便。
- 单元测试快。
- 少网络故障变量。

适合：

- 早期功能开发。
- controller/service 单元测试。
- 快速复现问题。

## 6. remote 模式

remote 模式调用链：

```text
Gateway
-> RemoteUserClient / RemoteMessageClient / RemoteFriendClient / RemoteGroupClient / RemotePushNotifier
-> gRPC
-> user_server / message_server / friend_server / group_server / push_server
-> Service Core
```

优点：

- 服务可以独立部署。
- 更接近分布式架构。
- 可以验证网络边界、序列化、错误映射。
- 为多机器测试做准备。

代价：

- 启动进程更多。
- 需要 endpoint 配置。
- 需要处理 gRPC 超时和错误。
- 测试和排查复杂度上升。

## 7. remote client 的职责

remote client 不只是 gRPC stub 包装，它还承担：

- 构造 proto request。
- 设置 deadline。
- 调用 gRPC。
- 判断 transport status。
- 判断业务 BaseResponse。
- proto DTO 转 service DTO。
- gRPC/业务错误映射为服务层错误码。

例如 RemoteMessageClient：

```text
SendRequest
-> im.message.SendMessageRequest
-> stub->SendMessage
-> im.message.SendMessageResponse
-> MessageData
```

这样 Gateway controller 不需要知道 proto 细节。

## 8. 错误处理边界

gRPC 调用有两层错误：

### 8.1 transport 错误

例如：

- 服务不可达。
- 连接超时。
- endpoint 配置错误。
- gRPC status 非 OK。

remote client 映射为：

```text
REMOTE_*_UNAVAILABLE
```

### 8.2 业务错误

例如：

- 参数错误。
- 用户不存在。
- 群不存在。
- 权限不足。

通过 proto `BaseResponse.error_code` 表达，再映射回服务层错误。

这种区分很重要：

```text
网络失败和业务拒绝不是一类问题。
```

## 9. remote-all 本地栈

当前脚本：

```text
scripts/dev/run_remote_services_stack.sh
```

启动：

```text
user_server:    127.0.0.1:9001
message_server: 127.0.0.1:9002
friend_server:  127.0.0.1:9003
group_server:   127.0.0.1:9004
push_server:    127.0.0.1:9101
gateway_server: HTTP 8102 / WebSocket 8101 / Push callback 9102
```

启动前：

```text
scripts/dev/prepare_runtime.sh
```

会准备：

- Redis。
- PostgreSQL。
- migrations。

这套 remote-all 是后续多机器测试的本地版本。

## 10. Push 的特殊远程边界

User/Message/Friend/Group 基本是：

```text
Gateway -> Service
```

Push 是双向边界：

```text
Gateway -> PushService.NotifyUser
PushService -> GatewayPushDeliveryService callback
```

原因：

- Push 决策可以远程。
- WebSocket session 必须留在 Gateway。
- 远程 Push 需要回调 Gateway 完成实际发送。

这比普通 CRUD gRPC 更有分布式设计含金量。

## 11. 当前优势

- Gateway 外部接口稳定。
- local/remote 可切换。
- 各服务有明确 `.proto` 契约。
- remote client 集中处理 proto 转换和错误映射。
- 支持本地多进程 remote-all 验证。
- Push callback 解决跨进程 WebSocket session 所有权问题。

## 12. 当前不足和后续优化

当前不足：

- endpoint 仍偏静态配置。
- 没有完整服务发现。
- 没有统一熔断、重试、负载均衡策略。
- 没有统一 trace id 贯穿 gRPC。
- 远程服务健康检查和自动恢复还不完整。
- proto 中还有部分未来字段和当前实现存在能力差异，需要文档中明确边界。

后续优化：

- 引入服务发现，例如 Consul/Etcd。
- Gateway 按服务名发现 endpoint。
- gRPC client 加统一 deadline、retry、circuit breaker。
- 加 trace id 和结构化日志。
- 增加健康检查和 readiness。
- 多机器部署验证。
- proto 按 MVP 和 future 字段分层整理。

## 13. 面试问答准备

### 13.1 为什么不一开始就全远程

回答思路：

```text
一开始全远程会增加调试和测试成本。MyChat 通过 facade 先保留 local 模式快速开发，
再用 remote client 切到 gRPC。这样既保证迭代效率，也保留分布式演进路径。
```

### 13.2 facade 解决了什么问题

回答思路：

```text
facade 把 Gateway controller 和具体调用方式解耦。
controller 不关心本地对象还是远程 gRPC，remote client 负责 proto 转换和错误映射。
```

### 13.3 gRPC 错误和业务错误怎么区分

回答思路：

```text
gRPC status 表示传输层是否成功，BaseResponse.error_code 表示业务是否成功。
网络不可达和参数错误不能混在一起处理。
```

### 13.4 Push 为什么是双向 gRPC

回答思路：

```text
Gateway 调 Push 是请求投递；Push 回调 Gateway 是因为 WebSocket session 在 Gateway 内存中。
远程 Push 不能直接访问 Gateway session，所以需要 GatewayPushDeliveryService。
```

### 13.5 后续多机器怎么做

回答思路：

```text
先把 remote-all 单机多进程跑稳，再把 user/message/friend/group/push/gateway 分配到不同机器。
接下来要补服务发现、配置管理、连接路由、日志追踪和健康检查。
```

## 14. 一句话总结

```text
MyChat 的 gRPC 设计不是简单“把函数远程化”，而是通过 Gateway facade 保持外部接口稳定，
用 `.proto` 明确内部服务契约，使系统能在 local 调试和 remote 分布式部署之间平滑切换。
```

## 附录：源码导读

### A. 源码阅读地图

| 服务 | proto | Gateway remote client | gRPC server |
| --- | --- | --- | --- |
| User | `common/proto/user.proto` | `gateway/http/remote_user_client.cpp` | `services/user/user_grpc_service.cpp` |
| Message | `common/proto/message.proto` | `gateway/http/remote_message_client.cpp` | `services/message/message_grpc_service.cpp` |
| Friend | `common/proto/friend.proto` | `gateway/http/remote_friend_client.cpp` | `services/friend/friend_grpc_service.cpp` |
| Group | `common/proto/group.proto` | `gateway/http/remote_group_client.cpp` | `services/group/group_grpc_service.cpp` |
| Push | `common/proto/push.proto` | `gateway/push/remote_push_notifier.cpp` | `services/push/push_grpc_service.cpp` |
| Gateway callback | `common/proto/push.proto` | push_server 内部调用 | `gateway/push/gateway_push_delivery_service.cpp` |

### B. remote client 通用代码模式

以 `RemoteUserClient::login_by_account` 为例：

```text
检查 client_ 是否存在
-> 构造 im::user::LoginRequest
-> 填 account/password
-> grpc::ClientContext context
-> apply_deadline(context)
-> client_->login(&context, rpc_request, &rpc_response)
-> if !status.ok(): REMOTE_USER_UNAVAILABLE
-> if base.error_code != SUCCESS: 映射业务错误
-> proto UserInfo -> UserProfile
```

其他 remote client 也是这个模式：

```text
service DTO -> proto request -> gRPC call -> BaseResponse -> service DTO
```

面试重点：

```text
remote client 是边界适配层，不是业务层。
```

### C. gRPC server 通用代码模式

以 `UserGrpcService::Login` 为例：

```text
检查 response/user_service/request
-> 从 proto request 提取 account/password
-> 参数校验
-> user_service_->login_by_account
-> result.ok ? SUCCESS : login_error_code(result.error_code)
-> fill_user_info
-> return grpc::Status::OK
```

注意这里：

- gRPC status 返回 OK，不代表业务成功。
- 业务成功/失败在 `BaseResponse.error_code`。
- 这让调用方可以区分传输层错误和业务错误。

### D. User remote 调用栈

```text
UserHttpController::handle_login
-> UserClient::login_by_account
-> RemoteUserClient::login_by_account
-> im.user.UserService::Stub::Login
-> UserGrpcService::Login
-> UserService::login_by_account
-> UserRepository::find_by_account
```

对应代码要点：

- Gateway controller 不知道 gRPC 细节。
- RemoteUserClient 处理 proto 转换。
- UserGrpcService 处理 proto 到 service request 的转换。

### E. Message remote 调用栈

```text
MessageHttpController::handle_send 或 MessageWsHandler::handle_send
-> MessageClient::send_text_message
-> RemoteMessageClient::send_text_message
-> im.message.MessageService::Stub::SendMessage
-> MessageGrpcService::SendMessage
-> MessageService::send_text_message
-> MessageRepository::create
```

阅读重点：

- `RemoteMessageClient::to_data` 把 proto `MessageBody` 转回 `MessageData`。
- `MessageGrpcService::fill_message_body` 把 service `MessageData` 转 proto。
- delivered/read 也有对应 RPC。

### F. Push remote 的特殊双向调用栈

普通服务是：

```text
Gateway -> remote service
```

Push 是：

```text
Gateway
-> RemotePushNotifier::notify_user
-> PushService.NotifyUser
-> push_server PushRuntime
-> GatewayPushDeliveryService.ListUserSessions
-> GatewayPushDeliveryService.SendSessionPayload
-> GatewayPushDeliveryService.MarkMessageDelivered
```

这体现：

- Push server 独立部署。
- Gateway 仍持有 WebSocket session。
- callback 是跨进程访问 Gateway 连接能力的边界。

### G. remote-all 脚本按代码读

```text
scripts/dev/run_remote_services_stack.sh
-> scripts/dev/prepare_runtime.sh
-> 检查 user/message/friend/group/push/gateway 二进制
-> 启动 user_server 9001
-> 启动 message_server 9002
-> 启动 friend_server 9003
-> 启动 group_server 9004
-> 启动 push_server 9101
-> 启动 gateway_server 8102/8101/9102
```

这就是本地多进程分布式验证入口。

### H. 面试追问如何指回代码

| 追问 | 指向代码 |
| --- | --- |
| local/remote 怎么切 | `Local*Client` vs `Remote*Client` |
| proto 契约在哪 | `common/proto/*.proto` |
| remote 错误怎么处理 | `Remote*Client` 中 `status.ok()` 与 `base.error_code()` |
| gRPC server 怎么复用 service | `*GrpcService` 调 `*Service` |
| Push 为什么特殊 | `RemotePushNotifier` + `GatewayPushDeliveryService` |
| remote-all 怎么跑 | `scripts/dev/run_remote_services_stack.sh` |
