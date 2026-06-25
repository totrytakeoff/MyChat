# Gateway TODO

## 2026-06-23 Gateway 重构收口结论

当前 Gateway 已经从旧 controller/client 入口回归到统一消息模型：

```text
HTTP / WebSocket
-> MessageParser
-> UnifiedMessage
-> MessageProcessor
-> GatewayCommandHandlerRegistry
-> local packet dispatcher 或 remote ForwardPacket gRPC
```

本轮必须先冻结当前重构成果，不再继续扩大拆改范围。后续优先级：

- [x] HTTP 业务 URL 已基于 `http_router.routes` 配置进入统一路由，不再依赖 `register_*_http_routes()` 硬编码业务入口。
- [x] `GatewayServer::init_server()` 已拆为数据库、user/message/friend/group/group-message、WS/push runtime 初始化函数。
- [x] `GatewayRuntimeRegistry` 已接管 user/message/friend/group 的 local service 与 remote adapter 生命周期。
- [x] `GatewayCommandHandlerContext` 已收窄为单一 `runtime` 指针，handler 不再持有展开式 service/adapter 字段。
- [ ] 保持当前主链路稳定，先推进回归测试、面试文档和最终总结文档更新。
- [ ] 后续有余力时再做 runtime endpoint map、packet proto 拆分、push/ws/auth 生命周期进一步收敛。

## 2026-06-22 Gateway 统一消息处理重构

基准文档:

- `docs/project/architecture/gateway_unified_message_refactor_baseline.md`
- `docs/project/architecture/gateway_handler_boundary_refactor_requirements.md`
- `docs/project/architecture/gateway_protobuf_packet_boundary_baseline.md`

核心目标:

- [x] HTTP 业务请求回归 `MessageParser -> UnifiedMessage -> MessageProcessor -> cmd handler`。
- [x] 停用 `register_*_http_routes()` 作为业务主入口。
- [x] 新增 `GatewayCommandHandlerRegistry` 集中注册业务命令。
- [x] 补全 `http_router.routes`，移除硬编码业务 URL 注册。
- [x] 修正 `command.proto` 与 route config 的语义冲突。
- [x] 将剩余 `Remote*Client` 重新命名为 remote service adapter，并迁出 `gateway/http`。
- [ ] 重写 final_sum_docs 中 controller 主入口相关描述。
- [x] 将当前桥接式 handler 改为直接依赖 service adapter，移除 controller 作为 handler 内部实现细节。
- [x] 删除旧 HTTP controller 源码和直接 mock controller 的单元测试。
- [x] 拆分 `GatewayCommandHandlerRegistry` 第一阶段结构：顶层 registry 只调模块 registrar，handler 实现按 user/message/friend/group 分文件，公共工具独立。
- [x] 移除 `Local*Client` 薄包装，local 模式下由 handler 直接调用 service/server。
- [x] 修复 ODB/libpq 静态链接依赖，恢复带服务完整 `gateway_server` 链接验证。
- [x] 完成 message 模块第一批 packet boundary 链路：`CMD_SEND_MESSAGE` / `CMD_MESSAGE_HISTORY` / `CMD_PULL_MESSAGE` 均由 Gateway 鉴权后转发 raw packet，Message service 侧解析 HTTP JSON / WS protobuf payload。
- [x] 完成 friend 模块 packet boundary 链路：`CMD_ADD_FRIEND` / `CMD_HANDLE_FRIEND_REQUEST` / `CMD_GET_FRIEND_LIST` / `CMD_GET_FRIEND_REQUESTS` 均由 Gateway 鉴权后转发 raw packet，Friend service 侧解析 HTTP JSON payload。
- [x] 完成 user 模块 packet boundary 链路：注册、登录、资料查询/更新、用户搜索均由 Gateway 转发 raw packet，User service 侧解析 HTTP JSON payload；Gateway 仅保留 token 签发这一鉴权边界。
- [x] 完成 group 模块 packet boundary 链路：建群、搜群、群资料、群列表、入群、退群、成员列表、群消息发送/历史均由 Gateway 转发 raw packet，Group service 侧解析 HTTP JSON payload 并产出 push event。
- [x] 删除 `gateway/command_handlers/response_mappers.*`，业务响应 JSON mapper 已迁入各 service packet dispatcher。

当前实现状态:

- 已新增 `gateway/command_handlers/GatewayCommandHandlerRegistry`，统一注册 auth/user、message、friend、group、group-message 的业务 cmd handler。
- 正式 HTTP server 不再注册 `register_*_http_routes()`，业务请求统一由 catch-all 进入 parser/processor。
- `GatewayCommandHandlerRegistry` 已不再回调 `UserHttpController` / `MessageHttpController` / `FriendHttpController` / `GroupHttpController` / `GroupMessageHttpController`。
- local 模式下，模块 handler 直接调用 `UserService` / `MessageService` / `FriendService` / `GroupService` / `GroupMessageService`；remote 模式下通过 `Remote*ServiceAdapter` gRPC adapter 调用远程服务。
- `RouterManager` 已支持 `method + path` 精确路由，并保留旧配置 `ANY path` fallback。
- `UnifiedMessage` 已补充 HTTP query 参数承载，GET 业务接口进入统一链路后仍可读取参数。
- `command.proto` 已新增 `CMD_SEARCH_GROUP` 与 `CMD_SEND_GROUP_MESSAGE`，并刷新生成代码。
- 已补齐 `config/dev.json`、`config/dev.remote-push.json`、`config/dev.remote-all.json`、`config/benchmark.json` 的 MVP HTTP route 映射。
- 旧 `register_*_http_routes_on_server()`、HTTP controller 文件、直接 mock controller 的旧单测已删除；当前业务 HTTP 入口只保留统一 catch-all。
- `LocalUserClient`、`LocalMessageClient`、`LocalFriendClient`、`LocalGroupClient` 及其 cpp 实现已删除；remote gRPC adapter 已迁入 `gateway/service_adapters/remote_*_service_adapter.*`。
- `GatewayCommandHandlerRegistry` 已完成拆分：顶层注册中心只调用 `register_user_command_handlers()`、`register_message_command_handlers()`、`register_friend_command_handlers()`、`register_group_command_handlers()`；模块 handler 主路径只保留依赖检查、鉴权、header 补齐、packet 转发、push event 转交和 HTTP/WS 响应回写。
- `MessageService` 已新增 `ForwardPacket(MessagePacketRequest) returns (MessagePacketResponse)` gRPC packet RPC；remote message adapter 的 message HTTP/WS 主链路已切到 packet forward，不再通过业务 RPC `SendMessage` / `GetConversation` / `PullOffline` 实现 Gateway message 请求。
- `services/message/message_packet_dispatcher.*` 已作为 message service 侧 packet dispatcher，负责解析 `CMD_SEND_MESSAGE`、`CMD_MESSAGE_HISTORY`、`CMD_PULL_MESSAGE` 的 `application/json` 与发送消息 protobuf payload，构造兼容 HTTP JSON 或 protobuf response，并返回 `MessagePushEvent` 供 Gateway 仅做推送转交。
- `FriendService` 已新增 `ForwardPacket(FriendPacketRequest) returns (FriendPacketResponse)` gRPC packet RPC；remote friend adapter 的 HTTP 主链路已切到 packet forward，不再通过业务 RPC `SendRequest` / `RespondToRequest` / `GetFriends` / `GetPendingRequests` 实现 Gateway friend 请求。
- `services/friend/friend_packet_dispatcher.*` 已作为 friend service 侧 packet dispatcher，负责解析好友申请、处理申请、好友列表、待处理申请的 `application/json` payload，并构造兼容 HTTP JSON 响应。
- `UserService` 已新增 `ForwardPacket(UserPacketRequest) returns (UserPacketResponse)` gRPC packet RPC；remote user adapter 的 HTTP 主链路已切到 packet forward。登录/注册成功后的 access/refresh token 仍由 Gateway 基于 service 返回的 auth hint 签发，这是鉴权边界，不属于用户业务解析。
- `services/user/user_packet_dispatcher.*` 已作为 user service 侧 packet dispatcher，负责解析注册、登录、资料查询/更新、用户搜索的 `application/json` payload，并构造兼容 HTTP JSON 响应与 auth token hint。
- `GroupService` 已新增 `ForwardPacket(GroupPacketRequest) returns (GroupPacketResponse)` gRPC packet RPC；remote group adapter 的 HTTP 主链路已切到 packet forward，不再通过业务 RPC 实现 Gateway group 请求。
- `services/group/group_packet_dispatcher.*` 已作为 group service 侧 packet dispatcher，负责解析群组/群消息 HTTP JSON payload，执行业务校验、群资料组合、群消息 fanout 成员计算，并返回 `GroupPushEvent` 供 Gateway 仅做推送转交。
- `gateway/command_handlers/response_mappers.*` 已删除；Gateway 中不再维护业务 DTO -> JSON 的响应 mapper。
- `gateway/ws/message_ws_handler.cpp` 的 `CMD_SEND_MESSAGE` 主链路已不再解析 `SendMessageRequest` payload；Gateway 本地拦截错误已改为 `PacketErrorBuilder` 构造 `application/json` envelope 错误包，不再构造业务型 `SendMessageResponse`。
- Gateway 与 service adapter 已直接使用 `common/proto` 中的 `UserPacketRequest/Response`、`MessagePacketRequest/Response`、`FriendPacketRequest/Response`、`GroupPacketRequest/Response`，`gateway/service_adapters/service_packet.hpp` 已删除。
- 各 service packet dispatcher 已直接返回 protobuf packet response，服务侧自定义 `XxxPacketResult` 已删除。
- `UserServiceAdapter`、`MessageServiceAdapter`、`FriendServiceAdapter`、`GroupServiceAdapter` 已删除历史业务方法，只保留 `forward(XxxPacketRequest)`。
- `GatewayCommandHandlerContext` 字段已去掉 `IM_ENABLE_*` 裁剪，user/message/friend/group registrar 默认注册命令并在运行时检查 local service 或 remote adapter 是否可用；`gateway/command_handlers` 仅剩 `IM_ENABLE_MESSAGE_WS` 用于隔离条件编译的 WS handler。

验证结果:

- 通过：`cmake --build /tmp/mychat-build-gcc-odb --target im_gateway_core -j2`，带 User/Message/Friend/Group/Push 服务分支的 Gateway core 可编译。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb --target gateway_server -j2`，带 ODB/service 的实际 Gateway 可执行目标可链接。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-tests --target test_gateway_message_ws test_push_service -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-tests -R 'GatewayMessageWsTest|PushServiceTest' --output-on-failure`，2/2 通过。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target test_remote_user_adapter test_remote_message_adapter test_remote_friend_adapter test_remote_group_adapter -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'Remote(User|Message|Friend|Group)ServiceAdapterTest' --output-on-failure`，4/4 通过。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target generate_proto -j2`，`message.proto` 的 `ForwardPacket` 生成链路已刷新。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target test_remote_message_adapter test_remote_message_gateway_server_smoke -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'RemoteMessage(ServiceAdapter|GatewayServerSmoke)Test' --output-on-failure`，2/2 通过。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-tests --target test_message_packet_dispatcher -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-tests -R 'Message(ServiceCore|PacketDispatcher)Test' --output-on-failure`。
- 边界扫描：`rg -n 'body\.value\(|SendRequest request|message_data_to_json|query_value\(|parse_limit\(|mark_delivered|get_conversation\(|pull_offline\(|peer_uid|before_time|limit' gateway/command_handlers/message_command_handlers.cpp` 无命中，message command handler 已不再解析 message 业务字段或调用 message 业务方法。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target test_friend_packet_dispatcher test_friend_service test_friend_grpc_service test_remote_friend_adapter test_remote_friend_gateway_server_smoke -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'Friend(ServiceCore|PacketDispatcher|GrpcService)Test|RemoteFriend(ServiceAdapter|GatewayServerSmoke)Test' --output-on-failure`，5/5 通过。
- 边界扫描：`rg -n 'body\.value\(|FriendRequest request|friend_info_to_json|parse_json_body|send_request\(|respond_to_request\(|get_friends\(|get_pending_requests\(|target_uid|friend_id|accept' gateway/command_handlers/friend_command_handlers.cpp` 无命中，friend command handler 已不再解析 friend 业务字段或调用 friend 业务方法。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target test_user_packet_dispatcher test_user_service test_user_grpc_service test_remote_user_adapter test_group_service test_group_packet_dispatcher test_group_grpc_service test_remote_group_adapter test_remote_group_gateway_server_smoke -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'User(ServiceCore|PacketDispatcher|GrpcService)Test|RemoteUserServiceAdapterTest|Group(ServiceCore|PacketDispatcher|GrpcService)Test|RemoteGroup(ServiceAdapter|GatewayServerSmoke)Test' --output-on-failure`，9/9 通过。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb --target gateway_server -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'Remote(Message|User|Friend|Group)ServiceAdapterTest|UserPacketDispatcherTest|MessagePacketDispatcherTest|FriendPacketDispatcherTest|GroupPacketDispatcherTest|GatewayMessageWsTest' --output-on-failure`，9/9 通过。
- 边界扫描：`rg -n 'to_gateway_result|ServicePacket|ServicePacketResult|service_packet\.hpp|send_text_message|get_conversation|pull_offline|mark_read|register_user\(|login_by_account|get_profile_by_uid|get_profile_by_account|search_profiles|update_profile|send_request\(|respond_to_request|get_friends|get_pending_requests|create_group\(|join_group\(|leave_group\(|list_my_groups|get_group_info\(|search_groups\(|group_exists\(|list_members\(|send_message\(|get_history\(' gateway/service_adapters gateway/command_handlers gateway/ws -g'*.hpp' -g'*.cpp'` 无命中，Gateway adapter/handler 主链路已不再暴露旧业务方法或旧 packet result。
- 边界扫描：`rg -n 'struct .*Packet|struct .*PacketResult|\b(User|Message|Friend|Group)PacketResult\b' services/*/*_packet_dispatcher.hpp services/*/*_packet_dispatcher.cpp gateway -g'*.hpp' -g'*.cpp'` 无命中，service packet dispatcher 已改为 protobuf packet contract。
- 边界扫描：`rg -n 'body\.value\(|parse_json_body|query_value\(|parse_limit\(|RegisterRequest|LoginRequest|UpdateProfileRequest|CreateGroupRequest|group_info_to_json|member_info_to_json|user_profile_to_json|search_users\(|get_user_info\(|update_user|search_groups\(|list_my_groups\(|get_group_info\(|join_group\(|leave_group\(|group_exists\(|list_members\(|send_message\(|get_history\(' gateway/command_handlers/*.cpp` 无命中，模块 handler 已不再解析 user/group/message/friend 业务字段或调用对应业务方法。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target test_gateway_message_ws -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'GatewayMessageWsTest' --output-on-failure`，15/15 通过。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'Remote(Message|User|Friend|Group)ServiceAdapterTest|UserPacketDispatcherTest|MessagePacketDispatcherTest|FriendPacketDispatcherTest|GroupPacketDispatcherTest|GatewayMessageWsTest' --output-on-failure`，9/9 通过。
- 边界扫描：`rg -n 'SendMessageResponse|message\.pb\.h' gateway/ws gateway/command_handlers gateway/service_adapters -g'*.cpp' -g'*.hpp'` 仅剩 `gateway/service_adapters/message_service_adapter.hpp` 对 `MessagePacketRequest/Response` 所在 `message.pb.h` 的 packet contract 依赖，`gateway/ws` 已无业务响应构造依赖。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb --target gateway_server -j2`，context/registrar 宏收敛后主 Gateway 可执行目标仍可链接。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target test_gateway_message_ws -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'GatewayMessageWsTest|Remote(Message|User|Friend|Group)ServiceAdapterTest|UserPacketDispatcherTest|MessagePacketDispatcherTest|FriendPacketDispatcherTest|GroupPacketDispatcherTest' --output-on-failure`，9/9 通过。
- 通过：`cmake --build /tmp/mychat-build-gcc-odb-grpc-tests --target gateway_server test_local_gateway_http_smoke -j2`。
- 通过：`ctest --test-dir /tmp/mychat-build-gcc-odb-grpc-tests -R 'Remote(User|Message|Friend|Group)GatewayServerSmokeTest|RemotePushGateway(Entrypoints|ServerSmoke)Test|LocalGatewayHttpSmokeTest' --output-on-failure`，7/7 通过。
- 再次通过：新增 `service_runtime_` 后重复执行上述 build + ctest，7/7 通过。
- 再次通过：新增 `GatewayRuntimeRegistry` 并收敛 handler context 后重复执行上述 build + ctest，7/7 通过。
- 再次通过：将 `GatewayRuntimeRegistry` 提升为 `GatewayServer` 长生命周期成员后重复执行上述 build + ctest，7/7 通过。
- 再次通过：将 user/message/friend/group service/adapter 所有权迁入 `GatewayRuntimeRegistry`，并将 `GatewayCommandHandlerContext` 收窄为单一 runtime 指针后重复执行上述 build + ctest，7/7 通过。

后续待收:

- [x] 按 `CMD_SEND_MESSAGE` 样板迁移 `CMD_MESSAGE_HISTORY` / `CMD_PULL_MESSAGE`，移除 Gateway 中的 `message_data_to_json`、离线 delivered 更新等 message 业务逻辑。
- [x] 按 message 样板迁移 friend HTTP handler，移除 Gateway 中的好友业务 JSON 解析、业务 DTO 构造和响应 mapper。
- [x] 将 user/group handler 中的 JSON 字段解析、业务 DTO 构造、响应 mapper 迁到对应 service packet dispatcher。
- [x] 将群消息 fanout、群资料组合等业务编排回收到 service/server。
- [x] 收敛 `MessageServiceAdapter` 中剩余业务方法，只保留 packet forward。
- [x] 收敛 `FriendServiceAdapter` 中剩余业务方法，只保留 packet forward。
- [x] 收敛 `UserServiceAdapter` / `GroupServiceAdapter` 中剩余业务方法，只保留 packet forward。
- [x] 移除 `gateway/command_handlers/response_mappers.*`，业务响应 mapper 已迁入 service packet dispatcher。
- [x] 收敛 `gateway/ws/message_ws_handler.cpp` 中业务型错误响应构造，改为统一 packet 错误 builder。
- [x] 第一阶段收敛 `GatewayCommandHandlerContext` 与模块 registrar 中的编译宏，命令注册进入运行时依赖检查。
- [x] 第一阶段拆分 `GatewayServer::init_server()`：数据库、user/message/friend/group/group-message、message WS/push 初始化已拆为独立 runtime init 方法。
- [x] 第一阶段收敛 `GatewayServer` local/remote 配置读取：新增统一 service runtime config helper，减少 `*.mode` / `*.timeout_ms` / unknown mode 判断重复代码。
- [x] 第二阶段新增 `GatewayServer::service_runtime_` 运行时服务状态表，统一记录 user/message/friend/group/push 的 mode、endpoint、timeout、local/remote bound 状态，并在 `/api/v1/stats` 输出。
- [x] 第三阶段引入 `GatewayRuntimeRegistry`，`GatewayCommandHandlerContext` 已从展开式 service/adapter 字段收敛为单一 `runtime` 指针。
- [x] 第四阶段将 `GatewayRuntimeRegistry` 提升为 `GatewayServer::runtime_registry_` 长生命周期成员，handler context 不再指向注册函数内的临时聚合对象。
- [x] 第五阶段将 user/message/friend/group 的 local service 与 remote adapter 所有权迁入 `GatewayRuntimeRegistry`，`GatewayServer` 不再维护同名散落成员镜像。
- [x] 收窄 `GatewayCommandHandlerContext` 为单一 `runtime` 指针，`auth_mgr` 统一从 `GatewayRuntimeRegistry` 获取，避免 handler 注册存在双来源依赖。
- [ ] 将 `GatewayServer` 初始化、成员生命周期和 local/remote 选择继续收敛为更泛化的 runtime service registry / endpoint map，减少 `IM_ENABLE_*` 宏散落。
- [ ] 评估 push/ws/auth 是否继续迁入 registry 所有权；当前保留裸指针是因为其生命周期仍由连接管理、WS handler 和远程 delivery server 初始化顺序约束。
- [ ] 拆分 packet contract proto：将 `XxxPacketRequest/Response` 从业务 proto 中迁出到 `*_packet.proto` 或统一 `gateway_packet.proto`，让 Gateway 编译期不接触业务 request/response 类型。
- [x] 补回统一入口 HTTP API 回归测试：新增 `test/gateway_local/test_local_gateway_http_smoke.cpp`，覆盖 local all-in-one HTTP 核心 IM 流程。
- [ ] 重写 `docs/final_sum_docs` 中 controller/facade 相关叙述。
- [x] 跑通 `gateway_server` 构建与 Gateway HTTP smoke 回归验证。

强约束:

- 不改前端 URL。
- 不改数据库 schema。
- 不破坏 HTTP keep-alive 修复。
- 不破坏 WSS inflight 背压、心跳和连接生命周期修复。
- 不再恢复 controller 入口；后续测试和文档必须围绕统一 parser/processor/handler 链路展开。

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
- [ ]  这些玩意儿就不应该存在,又把具体的服务逻辑引入gateway了! 应该统一在register中完成对应处理的!
 ```cpp 
        init_user_runtime();
        init_message_runtime();
        init_friend_runtime();
        init_group_runtime();
        init_group_message_runtime(); 
        init_push_runtime(); 
```
- [ ]
