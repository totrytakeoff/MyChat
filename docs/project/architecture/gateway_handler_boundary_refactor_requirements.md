# Gateway Handler 职责边界纠偏基准需求文档

日期：2026-06-22
更新：2026-06-23

## 1. 文档定位

本文档是 Gateway 统一消息处理重构后的职责边界基准。

它不替代 `gateway_unified_message_refactor_baseline.md`。前者回答“Gateway 主链路应该统一到哪里”，本文档回答“统一入口之后，handler、service adapter、service/server 各自承担什么职责”。

后续清理宏、收敛 service adapter 兼容接口、补回统一入口回归测试、重写面试文档，都必须以本文档为约束。

## 2. 当前状态

当前 Gateway 已完成入口纠偏：

```text
HTTP request / WebSocket frame
-> MessageParser
-> UnifiedMessage
-> MessageProcessor
-> GatewayCommandHandlerRegistry
-> module command handler
-> local packet dispatcher 或 remote packet RPC
```

已经完成的收口：

- 旧 HTTP controller 源码和直接 mock controller 的旧测试已删除。
- 正式 HTTP 业务请求不再绕过统一消息处理链路，统一由 catch-all 进入 parser/processor。
- `Local*Client` 薄包装已删除；local 模式由 handler 直接调用 service 侧 packet dispatcher。
- `Remote*Client` 已重命名并迁入 `gateway/service_adapters/remote_*_service_adapter.*`。
- user/message/friend/group/group-message 主链路已切到统一 packet boundary。
- `gateway/command_handlers/response_mappers.*` 已删除，业务 DTO 到 JSON 的响应映射已迁入各 service packet dispatcher。
- Gateway 模块 handler 当前主路径只保留依赖检查、鉴权、header 补齐、packet 转发、push event 转交和 HTTP/WS 响应回写。
- Gateway 与 remote service adapter 已直接使用 `common/proto` 中的 `XxxPacketRequest/XxxPacketResponse`，`ServicePacket/ServicePacketResult` 过渡结构已删除。
- `UserServiceAdapter`、`MessageServiceAdapter`、`FriendServiceAdapter`、`GroupServiceAdapter` 已删除历史业务方法，只保留 `forward(XxxPacketRequest)`。

仍需后续收口：

- `gateway/ws/message_ws_handler.cpp` 中 Gateway 本地拦截错误已统一为 `PacketErrorBuilder` 生成的 `application/json` envelope 错误包，WS handler 不再构造业务型 `SendMessageResponse`。
- `GatewayServer::init_server()` 已拆为分层 runtime init 方法，并统一了 service mode/timeout 读取；当前新增 `service_runtime_` 运行时状态表用于记录各服务 mode、endpoint、timeout、local/remote bound 状态，并引入 `GatewayServer::runtime_registry_` 作为 handler 运行时依赖入口。
- user/message/friend/group 的 local service 与 remote adapter 所有权已迁入 `GatewayRuntimeRegistry`；`GatewayCommandHandlerContext` 已收窄为单一 `runtime` 指针，不再维护第二份 `auth_mgr` 或展开式 service/adapter 字段。
- `docs/final_sum_docs` 中 controller/facade 相关表述需要重写。
- 统一入口 HTTP 回归测试已补回 local all-in-one smoke；后续可继续扩展异常路径和 WS+HTTP 混合端到端测试。

## 3. 原始设计原则

Gateway 的核心定位必须保持为：

```text
协议接入层 + 统一消息模型 + 命令分发层
```

目标链路：

```text
HTTP / WebSocket
-> MessageParser
-> UnifiedMessage
-> RouterManager
-> MessageProcessor
-> cmd_id handler
-> service/server packet boundary
-> ProcessorResult
-> HTTP response / WebSocket protobuf response
```

必要原则：

- Gateway 不是 controller 集合。
- Gateway 不沉淀领域业务流程。
- HTTP 和 WebSocket 必须共享统一命令分发模型。
- 新增业务能力主要通过 `cmd_id + route config + handler registration` 完成。
- local/remote 只是服务部署形态差异，不应改变 Gateway 主链路。
- Gateway handler 可以做协议适配和鉴权，但不能变成业务 service。

## 4. Handler 职责边界

Gateway handler 允许做：

- 校验 access token。
- 从 token 中解析 actor uid、device_id、platform。
- 用 token 中的 actor 身份覆盖 envelope/header 中的调用者字段。
- 构造并补齐统一 packet：`IMHeader + type_name + payload bytes`。
- HTTP 场景设置 `type_name = "application/json"` 并转发 raw body。
- WebSocket 场景转发 codec 解出的 `type_name + payload`。
- 消费 service 明确返回的边界元数据，例如 `MessagePushEvent`、`GroupPushEvent`。
- 调用 `PushNotifier` 做连接层推送交付。
- 将 service/server 返回的 packet payload 回写为 HTTP/WS 响应。
- user 登录/注册成功后，基于 user service 返回的 auth hint 签发 access/refresh token；这是 Gateway 鉴权边界，不是用户业务解析。

Gateway handler 禁止做：

- 解析业务 payload 字段，例如 `receiver_uid`、`target_uid`、`group_id`、`content`、`limit`。
- 构造业务 DTO，例如 `RegisterRequest`、`SendRequest`、`FriendRequest`、`CreateGroupRequest`。
- 把业务 DTO 映射为 JSON。
- 判断群是否存在、用户是否是群成员、好友关系是否成立。
- 决定群消息 fanout 目标成员。
- 拉取离线消息后逐条修改 delivered/read 状态。
- 跨多个领域 service 拼装业务结果。
- 维护业务状态流转规则。

判断标准：

```text
如果逻辑离开 Gateway 后仍然是业务规则，它就不应该写在 Gateway handler 中。
如果逻辑只和 HTTP/WS/token/cmd_id/packet/response 有关，它才属于 Gateway。
```

## 5. Registry 职责边界

`GatewayCommandHandlerRegistry` 是命令回调注册中心。

它应该负责：

- 接收 Gateway 初始化好的上下文。
- 调用各模块 registrar。
- 注册 `cmd_id -> callback`。
- 保证 HTTP 和 WebSocket 共用同一套 command dispatch。

它不应该负责：

- 直接实现 user/message/friend/group 业务 handler。
- 直接持有业务响应 mapper。
- 直接做复杂 local/remote 选择逻辑。
- 成为所有模块宏开关的集中污染点。

当前落地形态：

```text
gateway/command_handlers/
  gateway_command_handler_registry.*
  gateway_command_handler_modules.hpp
  command_handler_utils.*
  user_command_handlers.cpp
  message_command_handlers.cpp
  friend_command_handlers.cpp
  group_command_handlers.cpp
```

## 6. Adapter 层结论

### 6.1 Local service

`LocalUserClient`、`LocalMessageClient`、`LocalFriendClient`、`LocalGroupClient` 已删除。

当前要求：

- 不再新增 local thin wrapper。
- local 模式下，模块 handler 直接构造对应 protobuf `XxxPacketRequest`，并调用对应 packet dispatcher。
- 不改变现有 service 的领域行为和返回语义。

### 6.2 Remote adapter

`Remote*ServiceAdapter` 保留，因为它承担 transport 适配职责：

- 创建和持有 gRPC stub。
- 设置 RPC deadline。
- 接收并转发 protobuf packet request。
- 原样返回 protobuf packet response。
- 将 gRPC transport error 映射为稳定错误结果。
- 转交 service 返回的 push event/auth hint 等边界元数据。

当前要求：

- remote adapter 应位于 `gateway/service_adapters/`，不得放回 `gateway/http/`。
- remote adapter 不得重新实现业务字段解析和业务流程编排。
- remote adapter 不得暴露历史业务方法，例如 `send_text_message`、`register_user`、`create_group` 等。

## 7. 已回收到 service/server 的逻辑

以下逻辑已从 Gateway 主路径迁出：

- 单聊发送、历史拉取、离线拉取、delivered 更新。
- 好友申请、处理申请、好友列表、待处理申请。
- 用户注册、登录、资料查询/更新、用户搜索。
- 建群、搜群、群资料、群列表、入群、退群、成员列表。
- 群消息发送、群历史、群成员 fanout 目标计算。
- 业务 DTO 到 HTTP JSON 响应的 mapper。

这些逻辑当前位于：

```text
services/user/user_packet_dispatcher.*
services/message/message_packet_dispatcher.*
services/friend/friend_packet_dispatcher.*
services/group/group_packet_dispatcher.*
```

Gateway 可以触发 `PushNotifier`，但“应该推给谁”必须由 service 通过 push event 明确返回。

## 8. 功能启停与宏收敛要求

当前功能启停仍依赖 CMake target 和少量 `IM_ENABLE_*` 宏，这是短期可接受的工程现实，但宏不能继续污染主链路。

已完成的第一阶段收敛：

- `GatewayCommandHandlerContext` 字段不再被 `IM_ENABLE_*` 宏裁剪，context 成为稳定结构。
- user/message/friend/group 命令 handler 默认注册，服务是否可用由运行时 `local service || remote adapter` 检查决定。
- `gateway/command_handlers` 中仅 `message_command_handlers.cpp` 保留 `IM_ENABLE_MESSAGE_WS`，用于隔离条件编译的 `MessageWsHandler` 类型与方法调用。

后续要求：

- 功能是否启用，优先体现为对应模块 registrar 是否注册 handler。
- local/remote 选择应收敛在模块 registrar 或 adapter factory 中。
- `GatewayServer` 不应为每个业务模块散落大量初始化分支。
- `GatewayCommandHandlerContext` 后续应进一步替换为 runtime service registry / endpoint map，避免无限膨胀成所有服务对象的大杂烩。
- 顶层 registry 不应出现大量模块业务宏嵌套。

允许保留的宏位置：

- CMake target source/definition 控制。
- adapter factory 内部。
- 必要的类型声明隔离，例如当前 WS handler 条件编译。

## 9. 分阶段状态

### 第一阶段：统一入口

状态：已完成。

- HTTP 业务请求回归 `MessageParser -> UnifiedMessage -> MessageProcessor -> cmd handler`。
- 停用旧 `register_*_http_routes()` 业务主入口。
- 补齐 route config。
- 删除旧 HTTP controller 源码和旧 controller 直测。

### 第二阶段：结构拆分与 adapter 纠偏

状态：已完成。

- 顶层 registry 拆为模块 registrar。
- local thin wrapper 删除。
- remote client 重命名为 remote service adapter。
- `gateway/http/` 不再承载业务 client/controller。

### 第三阶段：packet boundary

状态：已完成。

- Gateway handler 不再解析业务字段。
- Gateway handler 不再构造业务 DTO。
- Gateway handler 不再维护业务 JSON mapper。
- 业务 JSON/protobuf payload 解析进入 service packet dispatcher。
- remote 模式通过 `ForwardPacket` gRPC RPC 进入同一 service packet dispatcher。
- service adapter 接口已收敛为 protobuf packet forward。

保留例外：

- `user_command_handlers.cpp` 使用 `nlohmann::json` 只用于登录/注册成功后向 service 返回的 HTTP JSON payload 注入 Gateway 签发的 token。该逻辑属于鉴权响应封装边界，不得扩展为用户业务字段解析。

### 第四阶段：最终清理

状态：部分完成。

- [x] 收敛 `gateway/ws/message_ws_handler.cpp` 业务型错误响应构造，改为 `common/network/PacketErrorBuilder`。
- [x] 第一阶段收敛 `GatewayCommandHandlerContext` 与 user/message/friend/group registrar 中的 `IM_ENABLE_*` 宏，handler 注册进入运行时依赖检查。
- [x] 补回统一入口 HTTP API 回归测试：`LocalGatewayHttpSmokeTest` 覆盖 local user/message/friend/group/group-message 核心流程。
- [x] 第一阶段收敛 `GatewayServer` 启动层：`init_server()` 拆为数据库、各业务 runtime、WS/push runtime 初始化方法，并统一读取 `*.mode` / `*.timeout_ms`。
- [x] 第二阶段新增 `GatewayServer::service_runtime_`，并在 `/api/v1/stats` 暴露各服务运行时模式与绑定状态。
- [x] 第三阶段引入 `GatewayRuntimeRegistry`，`GatewayCommandHandlerContext` 已收敛为 `runtime`，不再直接展开 user/message/friend/group/push/ws 字段。
- [x] 第四阶段将 `GatewayRuntimeRegistry` 提升为 `GatewayServer::runtime_registry_` 长生命周期成员。
- [x] 第五阶段将 user/message/friend/group local service 与 remote adapter 所有权迁入 `GatewayRuntimeRegistry`，并统一 handler 的 `auth_mgr` 来源。
- 重写 `docs/final_sum_docs` 中 controller/facade 相关叙述。
- 继续收敛 GatewayServer 初始化宏和 context 体积，目标是更泛化的 runtime service registry / endpoint map；下一步重点是把 push/ws/auth 的生命周期边界讲清楚，并逐步减少 `GatewayServer` 对具体服务类型的硬编码。

## 10. 验收标准

每个阶段完成后至少满足：

- `gateway_server` 可编译。
- HTTP 注册、登录、资料、用户搜索、好友、群组、单聊、群聊核心 API 不破坏。
- WebSocket 单聊发送、ack、push 不破坏。
- `gateway/command_handlers` 边界扫描不命中业务字段解析、业务 DTO 构造、业务 mapper。
- 远程 gRPC adapter 的 proto 转换逻辑不进入顶层 registry。
- 不恢复任何 controller 主入口。
- 不新增 local thin wrapper。
- 文档中不再把 Gateway 描述为 controller/facade 体系。

当前补充验证：

```bash
cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target gateway_server test_local_gateway_http_smoke -j2
ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests \
  -R 'Remote(User|Message|Friend|Group)GatewayServerSmokeTest|RemotePushGateway(Entrypoints|ServerSmoke)Test|LocalGatewayHttpSmokeTest' \
  --output-on-failure
```

结果：7/7 通过。

## 11. 强约束

- 不改前端 URL。
- 不改数据库 schema。
- 不破坏已有 WSS 心跳、inflight 背压、连接生命周期修复。
- 不破坏 HTTP keep-alive 和 stats 观测端点。
- 不把 gRPC/proto 细节泄漏进 Gateway 顶层注册中心。
- 不为了短期功能验证重新引入 controller 风格入口。
- 后续重构必须优先保持 MVP 可运行，再逐步回收职责边界。
