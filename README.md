# MyChat

MyChat 是一个基于 C++ 的分布式 IM 后端项目。当前项目已经进入完整 MVP 审视和面试准备阶段：后端核心链路已经具备账号、好友、单聊、群聊、实时推送和远程服务边界，Web 客户端用于验证后端能力。

## 当前定位

当前 MyChat 的重点不是做一个静态聊天界面，而是验证一套分布式 IM 后端工程能力：

- Gateway 统一暴露 HTTP 和 WebSocket。
- User、Message、Friend、Group、Push 保持独立服务边界。
- Gateway 通过 local/remote facade 屏蔽本地服务调用和远程 gRPC 调用差异。
- Redis 负责 token/session 等临时状态。
- PostgreSQL + ODB 负责用户、消息、好友、群组等持久业务数据。
- PushRuntime 和 fanout policy 负责在线推送选择、推送载荷构建和投递结果处理。
- React Web 客户端用于双账号、好友、单聊、群聊、实时推送等完整业务验证。

## 技术栈

- C++20
- CMake + vcpkg
- Boost.Asio / Boost.Beast
- Protobuf / gRPC
- Redis / hiredis
- PostgreSQL / ODB 2.5
- GoogleTest / CTest
- React + TypeScript + Vite
- Docker Compose

## 核心能力

- 注册、登录、token 鉴权、个人资料查询和更新。
- 单聊消息发送、历史查询、离线拉取、WebSocket 发送与 ack、在线实时推送。
- 好友搜索、好友申请、申请处理、好友列表。
- 群创建、群搜索、群资料、加入/退出、成员列表、群消息历史和群在线 fanout。
- User/Message/Friend/Group/Push 的 gRPC 服务边界和独立服务进程。
- 本地 remote-all 栈启动脚本和 Web 验证客户端。

## 快速启动

后端本地远程全栈：

```bash
scripts/dev/run_remote_services_stack.sh
```

Web 验证客户端：

```bash
cd clients/web
npm run dev -- --host 127.0.0.1
```

默认访问：

```text
http://127.0.0.1:5173/
```

Vite 会把 `/api/*` 代理到 Gateway HTTP `127.0.0.1:8102`，把 `/ws` 代理到 Gateway WebSocket TLS `127.0.0.1:8101`。

## 文档入口

- [文档总入口](docs/README.md)
- [项目总结分析与面试准备总规划](docs/final_sum_docs/00_项目总结分析与面试准备总规划.md)
- [当前进度与 MVP 状态](docs/project/roadmap/current_progress.md)
- [服务 MVP 路线图](docs/project/roadmap/service_mvp_roadmap.md)
- [MVP 架构说明](docs/project/architecture/mvp_architecture.md)
- [Web 验证客户端规划](docs/project/architecture/web_validation_client_plan.md)
- [模块文档入口](docs/modules/README.md)
- [Web 客户端说明](clients/web/README.md)

## 文档整理说明

旧的使用文档、阶段日志、早期参考资料和 codgent/agent 协作上下文已经归档：

- `docs/history/`
- `docs/reference/`
- `docs/modules/**/legacy_*`

这些历史资料只用于追溯，不作为当前项目现状的权威来源。当前项目说明以 `docs/project/`、`docs/final_sum_docs/`、`clients/web/README.md` 和当前代码为准。

## 后续方向

当前优先级是围绕已有 MVP 做总结、测试和面试准备：

- 重写当前版架构总览、模块说明和核心链路分析。
- 完成单机 Web 双账号验证文档。
- 准备多机器分布式部署验证方案。
- 沉淀简历项目描述、技术亮点和高频面试问答。

富媒体、消息撤回/编辑、复杂群管理、对象存储、生产级部署、服务发现、压测等内容作为后续扩展规划，不作为当前 MVP 阻塞项。
