# MyChat 文档总入口

本文档是当前 `docs` 目录的导航入口。整理后的原则是：当前项目文档和最终总结文档放在主路径，明显过时的旧说明、历史 devlog、codgent/agent 协作记录全部归档。

## 推荐阅读路径

### 快速了解当前项目

1. [项目总结分析与面试准备总规划](final_sum_docs/00_项目总结分析与面试准备总规划.md)
2. [当前进度与 MVP 状态](project/roadmap/current_progress.md)
3. [服务 MVP 路线图](project/roadmap/service_mvp_roadmap.md)
4. [MVP 架构说明](project/architecture/mvp_architecture.md)

### 准备简历和面试

所有新写的中文总结、技术分析、测试复盘、面试材料统一放在：

```text
docs/final_sum_docs
```

当前入口：

- [00_项目总结分析与面试准备总规划](final_sum_docs/00_项目总结分析与面试准备总规划.md)

### 了解当前工程文档

- `project/architecture/`：当前仍有效的架构、Web 验证客户端、面试复盘规划。
- `project/roadmap/`：当前进度、MVP 路线、阶段状态。
- `project/runtime/`：后续重写当前运行、部署、多机验证文档的目录。
- `modules/`：模块说明文档。目录内带 `legacy` 的文档只作为历史参考。

当前模块文档入口：

- [Gateway](modules/gateway/README.md)
- [User Service](modules/user.md)
- [Message Service](modules/message.md)
- [Friend Service](modules/friend.md)
- [Group Service](modules/group.md)
- [Push Service](modules/push.md)
- [存储与缓存](modules/storage.md)
- [Web 验证客户端](modules/web_client.md)

### 追溯历史

- `history/devlog/`：阶段开发日志。
- `history/codgent/`：早期 codgent 多 agent 实验输出。
- `history/agent_context/`：早期 agent/codgent 协作上下文、任务记录和运行日志。
- `history/root_old/`：原顶层旧报告。
- `reference/original/`：早期参考文档，可能与当前项目现状不一致。

历史文档保留用于追溯，不作为当前项目状态的权威来源。若历史文档中的路径、设计、接口和当前代码冲突，以 `project/`、`final_sum_docs/` 和当前代码为准。

## 当前目录职责

```text
docs/
  README.md                       文档总入口
  final_sum_docs/                 后续中文总结、测试复盘、面试准备主目录
  project/                        当前仍有效的项目级文档
  modules/                        模块级文档，legacy 内容仅供参考
  history/                        阶段日志与协作历史归档
  reference/                      早期参考资料和教程归档
  assets/                         图片等静态资源
```

## 后续维护规则

- 新的项目总结和面试准备材料写入 `final_sum_docs/`。
- 新的当前工程文档写入 `project/` 或 `modules/`。
- 失效但仍有参考价值的旧文档移入 `history/` 或 `reference/`。
- 不再把当前文档写入 `docs/docs`、`docs/devlog`、`docs/agent_context` 这类旧路径。
- 修改代码导致文档失效时，优先重写当前版文档；旧版说明归档，不做局部补丁式维护。
