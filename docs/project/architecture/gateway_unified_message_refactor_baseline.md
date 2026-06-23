# Gateway 统一消息处理重构基准文档

日期：2026-06-22

## 1. 文档定位

本文档是 Gateway 架构回归重构的基准文档。

后续所有重构实现、测试验收、TODO 更新、`final_sum_docs` 重写，都以本文档为准。它解决四个问题：

- 当前 Gateway 真实现状是什么。
- 最初 Gateway 架构设计的核心意图是什么。
- 这次重构要回到什么目标形态。
- 哪些约束、需求、验收标准不能被实现过程稀释。

这不是普通 TODO，也不是事后总结。它是进入重构前的“现状-目标-要求-需求”冻结文档。

## 2. 基准信息

当前工作分支：

```text
dev
```

当前重构起点 commit：

```text
4b18c7d Document benchmark baseline and validation guides
```

旧架构参考仓库：

```text
/home/myself/temp/MyChat
```

旧架构参考 commit：

```text
f82f6112406013a9af60b78431d8a91413e0e18a
完成了pgsql odb连接的相关封装，开始封装 pgsql连接池管理类
```

本轮重构分支目标：

```text
在 dev 分支上完成 Gateway 主链路回归，不污染 master 上已提交的稳定文档与压测基线。
```

## 3. 当前项目阶段

MyChat 当前已经进入完整 MVP 后的总结、面试准备和稳定性收口阶段。

当前已经具备：

- Gateway HTTP / WebSocket 接入。
- User / Message / Friend / Group / Push 服务边界。
- local / remote gRPC 双模式。
- Redis token/session 状态。
- PostgreSQL/ODB 持久化。
- React Web 客户端验证。
- 基础压测报告和性能分析文档。

当前不是从零开发，也不是功能探索阶段。本次重构必须在已有 MVP 能力完整保留的前提下进行。

## 4. 当前性能与功能基线

当前可引用完整压测报告：

```text
docs/benchmark/benchmark_report_20260622_full_retest/benchmark_report_20260622_full_retest.md
```

当前性能基线：

| 场景 | 当前数据 | 判断 |
| --- | ---: | --- |
| HTTP ramp | 500 VU，46230 请求，失败率 0%，p95 5.81ms | HTTP 接入层稳定 |
| WSS 建连 | 200 连接，失败 0，connect p95 约 15ms | 200 连接规模稳定 |
| WSS 消息 | 200 用户 / 1s，RTT p95 12.60ms | 常规聊天压力稳定 |
| WSS 消息 | 200 用户 / 500ms，RTT p95 21.92ms | 高频聊天仍流畅 |
| WSS 极限 | 200 用户 / 250ms，RTT p95 2628.05ms | 当前吞吐拐点 |

当前功能基线：

- 注册、登录、资料查询、资料更新。
- 用户搜索。
- 好友申请、申请处理、好友列表、待处理申请。
- 单聊 HTTP 发送、WebSocket 发送、历史消息、离线消息、在线推送。
- 创建群、搜索群、查看群资料、加群、退群、群成员、群消息发送、群消息历史。
- remote-all 形态可用于分布式服务边界验证。
- Web 双账号可验证主要 IM 用户流程。

重构后不能明显破坏上述基线。性能极限场景可以后续优化，但稳定区间不能出现明显退化。

## 5. 原始架构设计意图

最初 Gateway 设计的核心思想是：Gateway 是协议接入层和命令分发层，不是一组 HTTP controller 的集合。

目标链路：

```text
HTTP request / WebSocket frame
-> MessageParser
-> UnifiedMessage
-> RouterManager
-> MessageProcessor
-> cmd_id handler
-> service call
-> ProcessorResult
-> HTTP response / WebSocket protobuf response
```

核心组件职责：

| 组件 | 原始职责 |
| --- | --- |
| `MessageParser` | 将 HTTP 和 WebSocket 输入解析为统一消息，不做业务 |
| `UnifiedMessage` | 承载 cmd_id、token、device、platform、HTTP body、WS protobuf payload、session context |
| `RouterManager` | 基于 HTTP path 或 cmd_id 解析服务名和命令 |
| `MessageProcessor` | 根据 cmd_id 查找并执行注册的处理函数 |
| cmd handler | 执行具体业务编排，返回 `ProcessorResult` |
| GatewayServer | 初始化组件、接入协议、调用 parser/processor、写回响应 |

旧参考 commit 中，HTTP 业务入口是 catch-all：

```text
http_server_->Get(".*", http_callback)
http_server_->Post(".*", http_callback)
```

而不是每个模块单独注册 controller route。

旧设计的扩展方式：

```text
新增业务能力
-> 增加或复用 command.proto 中的 cmd_id
-> 增加 HTTP route config
-> 注册 cmd_id handler
-> handler 调用目标 service
```

这就是本次重构要恢复的核心。

## 6. 当前实际架构现状

当前 `dev` 分支中，统一消息处理骨架仍然存在：

- `gateway/message_processor/message_parser.hpp/.cpp`
- `gateway/message_processor/unified_message.hpp`
- `gateway/message_processor/message_processor.hpp/.cpp`
- `gateway/router/router_mgr.hpp`
- `gateway/router/router.mgr.cpp`

重构前 WebSocket 主链路接近目标设计：

```text
WebSocket frame
-> MessageParser::parse_websocket_message_enhanced
-> ThreadPool inflight control
-> MessageProcessor::process_message_sync
-> registered cmd handler
-> WebSocket response
```

但重构前 HTTP 主业务链路已经偏移为：

```text
explicit httplib route
-> UserHttpController / MessageHttpController / FriendHttpController / GroupHttpController / GroupMessageHttpController
-> XxxClient / RemoteXxxClient
-> local service 或 remote gRPC service
```

重构前 `GatewayServer::init_http_server()` 中，业务显式 route 注册早于 catch-all：

```text
register_user_http_routes()
register_message_http_routes()
register_friend_http_routes()
register_group_http_routes()
register_group_message_http_routes()

http_server_->Get(".*", http_callback)
http_server_->Post(".*", http_callback)
```

因此正常业务 HTTP 请求会先命中 controller，绕过：

```text
MessageParser -> UnifiedMessage -> MessageProcessor
```

## 7. 当前偏移造成的问题

### 7.1 架构表达偏移

原设计可以简洁表达为：

```text
协议输入统一转 UnifiedMessage，再按 cmd_id 分发 handler。
```

当前实际表达变成：

```text
HTTP 走 controller/client，WS 走 parser/processor，Push 又有 callback。
```

这会导致面试叙事变乱，Gateway 的亮点被 controller/facade 稀释。

### 7.2 扩展方式偏移

原设计新增业务只需要：

```text
cmd_id + route config + handler
```

当前新增业务需要：

```text
controller method
client interface
local client
remote client
explicit route registration
GatewayServer 宏控制
测试 mock
文档同步
```

这与最初“维护回调即可统一管理服务调用”的设计目标相反。

### 7.3 路由配置失效

当前 `http_router.routes` 不完整，已有大量前端正在使用的 HTTP API 不在配置中。

已知问题：

- `/auth/register` 缺失或不完整。
- `/users/search` 缺失。
- friends、groups、group messages 大量路由缺失。
- `/auth/refresh -> 1003` 与 `CMD_REGISTER=1003` 语义冲突。

路由配置不完整导致 `RouterManager` 在 HTTP 主业务链路中无法发挥作用。

### 7.4 命名和目录污染

重构中期 `gateway/http/*_client` 实际已经承担 local/remote service adapter 职责，但路径在 `gateway/http` 下，容易让人误解为只服务旧 HTTP controller。

当前已完成目录收口：service adapter 接口和 remote gRPC adapter 均迁入 `gateway/service_adapters/`，`gateway/http/` 只保留 HTTP protocol helper。

合理定位应该是：

```text
command handler
-> service adapter
-> local service 或 remote gRPC
```

而不是：

```text
HttpController
-> HttpClient
```

## 8. 重构目标形态

目标主链路：

```text
HTTP / WebSocket
-> protocol adapter
-> MessageParser
-> UnifiedMessage
-> RouterManager / cmd_id
-> MessageProcessor
-> GatewayCommandHandlerRegistry
-> command handler
-> service adapter
-> local service 或 remote gRPC
```

目标职责划分：

| 层 | 职责 |
| --- | --- |
| protocol adapter | 只处理 httplib/beast 这种协议库对象和响应写回 |
| `MessageParser` | 统一解析 HTTP/WS 为 `UnifiedMessage` |
| `RouterManager` | 基于配置解析 HTTP path 与 service/cmd |
| `MessageProcessor` | 维护 cmd handler 表，执行 handler |
| `GatewayCommandHandlerRegistry` | 集中注册所有业务命令 |
| command handler | 参数解析、token 上下文校验、调用 service adapter、构造结果 |
| service adapter | 屏蔽 local service 和 remote gRPC 差异 |

目标状态：

- HTTP 和 WS 都以 `UnifiedMessage` 作为中间表示。
- 业务分发统一基于 `cmd_id`。
- GatewayServer 不维护每个业务模块的显式 HTTP route。
- Controller 不再作为主业务入口。
- local/remote 能力保留，但改定位为 service adapter。
- 前端 API URL 保持不变。
- WebSocket 的 inflight 背压、心跳、连接清理保留。
- HTTP 的 keep-alive worker 修复和 `/api/v1/stats` 保留。

## 9. 强约束

实现过程中必须遵守：

- 不改前端 HTTP URL。
- 不改数据库 schema。
- 不重写 User/Message/Friend/Group/Push service 内部业务逻辑。
- 不重写 gRPC server。
- 不破坏 HTTP keep-alive worker 修复。
- 不破坏 WebSocket inflight 背压、心跳、连接生命周期修复。
- 不删除 `/api/v1/health`。
- 不删除 `/api/v1/stats`。
- 第一阶段不做大规模目录搬迁。
- controller 入口不再保留，后续不能恢复模块级显式业务路由。
- 所有新增业务 handler 必须返回 `ProcessorResult`。
- 所有业务入口必须能从 cmd_id 找到对应 handler。
- 错误响应必须继续对前端友好，不能只暴露内部错误码。

## 10. 非目标范围

本次重构不解决：

- 多 Gateway 全局连接路由。
- 服务发现和负载均衡。
- 消息队列。
- 富媒体消息。
- 消息撤回、编辑、删除。
- 大群 fanout 优化。
- 250ms 极限压测场景性能优化。
- Web UI 改版。
- 数据库表结构调整。
- 完整生产部署编排。

这些内容只作为后续扩展方向，不阻塞本次主链路回归。

## 11. MVP 功能需求清单

重构后必须覆盖当前所有 MVP 功能。

账号与用户：

- 注册。
- 登录。
- 当前用户资料查询。
- 用户资料更新。
- 用户搜索。

好友：

- 好友申请。
- 好友申请接受/拒绝。
- 好友列表。
- 待处理好友申请。

单聊：

- HTTP 单聊发送。
- WebSocket 单聊发送。
- 单聊历史。
- 离线消息。
- 在线 push。

群聊：

- 创建群。
- 搜索群。
- 查看群资料。
- 加群。
- 退群。
- 群成员列表。
- 群消息发送。
- 群消息历史。
- 群消息在线 fanout。

连接与推送：

- WebSocket 建连。
- WebSocket 心跳。
- token/device 校验。
- remote Push callback。
- `/api/v1/health`。
- `/api/v1/stats`。

## 12. HTTP route 与 cmd_id 映射要求

以下映射必须进入 `http_router.routes` 并与 `command.proto` 保持一致。

| HTTP API | cmd_id | 说明 |
| --- | --- | --- |
| `POST /api/v1/auth/register` | `CMD_REGISTER` | 注册 |
| `POST /api/v1/auth/login` | `CMD_LOGIN` | 登录 |
| `GET /api/v1/auth/info` | `CMD_GET_USER_INFO` | 当前用户资料 |
| `POST /api/v1/auth/profile` | `CMD_UPDATE_USER_INFO` | 更新资料 |
| `GET /api/v1/users/search` | `CMD_SEARCH_USER` | 用户搜索 |
| `POST /api/v1/friends/request` | `CMD_ADD_FRIEND` | 好友申请 |
| `POST /api/v1/friends/respond` | `CMD_HANDLE_FRIEND_REQUEST` | 处理好友申请 |
| `GET /api/v1/friends` | `CMD_GET_FRIEND_LIST` | 好友列表 |
| `GET /api/v1/friends/pending` | `CMD_GET_FRIEND_REQUESTS` | 待处理申请 |
| `POST /api/v1/messages/send` | `CMD_SEND_MESSAGE` | 单聊发送 |
| `GET /api/v1/messages/history` | `CMD_MESSAGE_HISTORY` | 单聊历史 |
| `GET /api/v1/messages/offline` | `CMD_PULL_MESSAGE` | 离线消息 |
| `POST /api/v1/groups` | `CMD_CREATE_GROUP` | 创建群 |
| `GET /api/v1/groups/search` | `CMD_SEARCH_GROUP` | 群搜索 |
| `GET /api/v1/groups/info` | `CMD_GET_GROUP_INFO` | 群资料 |
| `GET /api/v1/groups` | `CMD_GET_GROUP_LIST` | 我的群列表 |
| `POST /api/v1/groups/join` | `CMD_APPLY_JOIN_GROUP` | 加群 |
| `POST /api/v1/groups/leave` | `CMD_QUIT_GROUP` | 退群 |
| `GET /api/v1/groups/members` | `CMD_GET_GROUP_MEMBERS` | 群成员 |
| `POST /api/v1/groups/messages/send` | `CMD_SEND_GROUP_MESSAGE` | 群消息发送 |
| `GET /api/v1/groups/messages/history` | `CMD_GET_GROUP_MESSAGES` | 群消息历史 |

命令表修正要求：

- `/auth/refresh` 不能继续映射到 `1003`，因为 `1003` 是 `CMD_REGISTER`。
- 群搜索已新增 `CMD_SEARCH_GROUP`，与“我的群列表”区分。
- 群消息发送已新增 `CMD_SEND_GROUP_MESSAGE`，不再复用“获取群消息”语义。

## 13. Handler 设计要求

新增 `GatewayCommandHandlerRegistry`，集中注册所有业务 handler。

建议上下文结构：

```cpp
struct GatewayCommandContext {
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr;
    std::shared_ptr<UserServiceAdapter> user;
    std::shared_ptr<MessageServiceAdapter> message;
    std::shared_ptr<FriendServiceAdapter> friend_;
    std::shared_ptr<GroupServiceAdapter> group;
    std::shared_ptr<PushNotifier> push;
    ConnectionManager* connection_manager;
};
```

注册入口：

```cpp
void register_gateway_command_handlers(
    MessageProcessor& processor,
    GatewayCommandContext context);
```

handler 统一签名：

```cpp
ProcessorResult(const UnifiedMessage& msg)
```

handler 责任：

- 从 `UnifiedMessage` 读取 token、body、query、path、protobuf payload。
- 校验 token 和 device/platform 约束。
- 从 token 中提取真实 uid，不信任客户端传入 uid。
- 调用 service adapter。
- 构造 JSON 或 protobuf 响应。

handler 不应该：

- 直接操作数据库连接。
- 直接依赖 httplib。
- 直接依赖 WebSocketSession。
- 写死 HTTP URL。
- 处理 service 内部状态机。

## 14. Service adapter 定位

本轮已经将历史 client 能力重新定义为 service adapter：

```text
service adapter = command handler 后面的服务调用适配层
```

当前文件位置：

```text
gateway/service_adapters/*_service_adapter.hpp
gateway/service_adapters/remote_*_service_adapter.hpp/.cpp
```

local thin wrapper 已删除；local 模式直接调用 service/server，remote 模式通过 `Remote*ServiceAdapter` 做 gRPC/proto/deadline/error mapping 适配。

## 15. 分阶段实施计划

### 阶段一：基准冻结

- 落本文档。
- 在 `docs/TODO/Gateway.md` 中记录重构任务索引。
- 确认 `dev` 分支干净。

### 阶段二：路由与命令表修复

- [x] 补全 `config/dev.json`。
- [x] 补全 `config/dev.remote-push.json`。
- [x] 补全 `config/dev.remote-all.json`。
- [x] 补全 `config/benchmark.json`。
- [x] 修正 `/auth/refresh` cmd 冲突。
- [x] 补 `command.proto` 中的群搜索和群消息发送命令。

### 阶段三：建立 registry

- [x] 新增 `GatewayCommandHandlerRegistry`。
- [x] 将 HTTP MVP 业务命令集中注册到 registry。
- [x] `CMD_SEND_MESSAGE` handler 内部按 HTTP / WebSocket 协议分流，保留现有 WS send 处理。
- [x] 保持现有 WS inflight 背压路径不变。

### 阶段四：迁移 HTTP 业务

迁移顺序：

1. auth / user。
2. message。
3. friend。
4. group。
5. group message。

每迁移一个模块：

- 对应业务 HTTP route 不再显式注册。
- 对应 handler 由 registry 注册。
- 对应测试改为验证统一入口。

当前直接 handler 实现状态：

- 正式 `GatewayServer::init_http_server()` 不再注册 `register_user_http_routes()`、`register_message_http_routes()`、`register_friend_http_routes()`、`register_group_http_routes()`、`register_group_message_http_routes()`。
- 业务 HTTP 请求统一命中 catch-all，再进入 `MessageParser::parse_http_request_enhanced()` 与 `MessageProcessor::process_message()`。
- `GatewayCommandHandlerRegistry` 已直接注册 auth/user、message、friend、group、group-message 的 command handler。
- command handler 直接依赖 `UserServiceAdapter` / `MessageServiceAdapter` / `FriendServiceAdapter` / `GroupServiceAdapter` 抽象，local 模式调用 service/server，remote 模式调用 `Remote*ServiceAdapter`。
- HTTP 参数解析、token 校验、JSON 响应封装已从 controller bridge 下沉到 command handler 内部，正式业务链路不再回调 `UserHttpController` / `MessageHttpController` / `FriendHttpController` / `GroupHttpController` / `GroupMessageHttpController`。
- `CMD_SEND_MESSAGE` 在同一个 command handler 内按 `msg.is_websocket()` / `msg.is_http()` 分流：WebSocket 继续复用已验证的 `MessageWsHandler`，HTTP 直接调用 `MessageServiceAdapter` 并触发 PushNotifier。
- 旧 `register_*_http_routes_on_server()` seam、HTTP controller 文件、直接 mock controller 的旧单测已删除，不再作为迁移参照保留在源码中。
- `RouterManager` 已支持 `method + path` 精确路由，并保留旧配置 `ANY path` fallback。
- `ServiceRouter::find_service(cmd_id)` 已优先使用 `http_router.routes` 中的精确 `cmd_id -> service_name` 映射，避免 `CMD_SEARCH_USER=3006` 这类历史编号被粗略 `cmd_range` 误归到 friend service。
- `UnifiedMessage::SessionContext` 已补充 HTTP query 参数承载，GET 业务接口进入统一链路后仍可读取参数。
- service adapter 已迁入 `gateway/service_adapters/`，下一阶段要补回统一入口 HTTP API 回归测试，并继续将 handler 内业务编排回收到 service/server。

当前验证结果：

- `cmake --build build-gcc --target gateway_server -j$(nproc)` 通过，验证无服务/ODB分支可完整构建。
- `cmake --build /tmp/mychat-build-gcc-odb --target im_gateway_core -j2` 通过，验证带 User/Message/Friend/Group/Push 服务分支的 Gateway core 可编译。
- `cmake --build /tmp/mychat-build-gcc-odb --target gateway_server -j2` 通过，验证带 ODB/service 的实际 Gateway 可执行目标可链接。
- `cmake --build /tmp/mychat-build-gcc-odb-tests --target test_gateway_message_ws test_push_service -j2` 通过。
- `ctest --test-dir /tmp/mychat-build-gcc-odb-tests -R 'GatewayMessageWsTest|PushServiceTest' --output-on-failure` 通过，2/2。
- `cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target test_remote_user_adapter test_remote_message_adapter test_remote_friend_adapter test_remote_group_adapter -j2` 通过。
- `ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'Remote(User|Message|Friend|Group)ServiceAdapterTest' --output-on-failure` 通过，4/4。

### 阶段五：HTTP / WS 合流

- `CMD_SEND_MESSAGE` 同时支持 HTTP 和 WS。
- handler 内部按 `msg.is_http()` / `msg.is_websocket()` 处理输入差异。
- 确保 ack、push、持久化语义不变。

### 阶段六：旧代码清理

- controller 已删除，不再恢复。
- `register_*_http_routes()` 已删除。
- `gateway/http/*_client` 已迁移到 `gateway/service_adapters/` 并重命名为 service adapter。

### 阶段七：文档重写

- 重写 Gateway 文档。
- 重写各业务链路文档。
- 重写面试问答中 controller/facade 相关描述。

## 16. 测试验收标准

### 16.1 编译验收

至少通过：

```bash
cmake --build build/remote-push-odb --target gateway_server -j2
```

以及 gateway 相关测试目标。

### 16.2 功能验收

必须通过：

- 注册、登录、资料查询、资料更新。
- 用户搜索。
- 好友申请、接受/拒绝、好友列表、待处理列表。
- 单聊 HTTP 发送、WS 发送、历史、离线。
- 在线 push 不需要手动刷新。
- 创建群、搜群、群资料、加群、退群、成员列表。
- 群消息发送、群消息历史、在线 fanout。
- remote-all 模式启动并完成核心链路。

### 16.3 架构验收

必须满足：

- 业务 HTTP 请求命中统一 `MessageParser / MessageProcessor`。
- `register_*_http_routes()` 不再作为业务入口。
- 所有业务 handler 均由 registry 注册。
- `http_router.routes` 覆盖所有现有业务 HTTP API。
- `command.proto` 与路由配置语义一致。
- `/api/v1/health`、`/api/v1/stats` 保持显式路由。

### 16.4 性能验收

轻量回归必须满足：

- HTTP ramp 不出现大面积 4xx/5xx。
- WSS 200 建连仍保持失败 0。
- WSS 200 用户 / 1s 和 / 500ms 稳定区不明显劣化。
- 不引入线程爆炸、连接泄漏、明显死锁。

`200 users / 250ms` 极限场景可以暂不优化，但不得比当前架构更不可控。

## 17. 文档重写范围

重构完成后必须同步改写：

- `docs/final_sum_docs/04_Gateway统一接入与服务编排分析.md`
- `docs/final_sum_docs/05_注册登录与鉴权链路分析.md`
- `docs/final_sum_docs/06_单聊消息链路分析.md`
- `docs/final_sum_docs/07_好友关系链路分析.md`
- `docs/final_sum_docs/08_群聊链路分析.md`
- `docs/final_sum_docs/09_实时推送与Push服务分析.md`
- `docs/final_sum_docs/10_gRPC服务边界与远程调用分析.md`
- `docs/final_sum_docs/17_简历项目描述与亮点.md`
- `docs/final_sum_docs/18_项目面试自我介绍.md`
- `docs/final_sum_docs/19_高频面试问题与回答.md`
- `docs/final_sum_docs/20_架构追问与技术取舍.md`

文档改写原则：

- 删除“controller 是主入口”的描述。
- 改为“Gateway command handler 是主入口”。
- local/remote client 改称 service adapter 或 remote adapter。
- 面试叙事聚焦统一消息模型、cmd handler 注册、协议入口统一。

## 18. 风险与控制

主要风险：

- HTTP 业务迁移时破坏前端兼容。
- token 校验逻辑在 controller 迁移到 handler 时遗漏。
- HTTP 与 WS 共用命令后响应格式混乱。
- route config 与 command.proto 不一致。
- 测试仍然 mock controller，导致假通过。

控制方式：

- 每次只迁移一个业务模块。
- controller 已删除；如需参照旧实现，通过 git 历史查看，不在当前源码中保留。
- 每个 handler 明确 HTTP JSON 响应和 WS protobuf 响应。
- 路由配置和 command.proto 同步修改。
- 测试必须走统一入口，不再直接测 controller。

## 19. 最终面试叙事目标

重构完成后，Gateway 可以这样讲：

```text
Gateway 不是一组 controller，而是统一协议接入和命令分发层。
HTTP 和 WebSocket 进入 Gateway 后都会转换成 UnifiedMessage。
RouterManager 根据 HTTP path 或 protobuf header 得到 cmd_id。
MessageProcessor 根据 cmd_id 调用注册的 command handler。
handler 再通过 service adapter 调用本地 service 或远程 gRPC。
新增业务命令时，主要维护 command.proto、路由配置和 handler 注册。
```

这才是 MyChat Gateway 作为简历项目的核心架构亮点。
