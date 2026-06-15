# 存储与缓存模块说明

## 当前职责划分

MyChat 当前使用 Redis 和 PostgreSQL 分别承担不同类型的数据。

Redis 负责高频、临时、可过期状态：

- access token 撤销状态。
- refresh token 元数据。
- 用户 token 索引。
- 会话/连接相关状态。
- 连接池复用。

PostgreSQL 负责持久业务状态：

- 用户账号和资料。
- 单聊消息。
- 好友申请和好友关系。
- 群资料。
- 群成员。
- 群消息。

ODB 负责 C++ 对象到 PostgreSQL 表的映射。

## Redis

当前 Redis 封装基于 hiredis，并使用 RAII 连接池。

关注点：

- pool size 可配置。
- pool wait timeout 可配置。
- 连接异常时支持重连和一次重试。
- token/session 这类状态天然适合 TTL 和快速访问。

面试可讲点：

- 为什么 token/session 不直接放 PostgreSQL。
- Redis TTL 对 refresh token 和撤销列表的意义。
- 连接池如何避免频繁建立连接。
- Redis 故障时当前系统的影响边界。

## PostgreSQL/ODB

当前持久化通过 PostgreSQL + ODB 完成。

相关模型位于：

```text
services/odb/user.hpp
services/odb/message.hpp
services/odb/friend.hpp
services/odb/group.hpp
services/odb/group_message.hpp
```

当前迁移基线：

```text
db/migrations/001_core_schema.sql
scripts/db/migrate_postgres.sh
```

服务仓储当前主要直接使用 `odb::pgsql::database`。`PgSqlConnection` 已有修复和测试，但不应在没有专门计划时大规模迁移现有 repository。

## 迁移策略

当前策略：

- 服务启动不自动执行迁移。
- 本地运行前由脚本显式准备运行时环境和迁移。
- 测试使用共享 schema helper，避免到处复制建表 SQL。

这样做的好处：

- 服务启动行为更可控。
- 迁移失败不会被业务服务静默吞掉。
- 多服务部署时迁移职责更清晰。

## 面试可讲点

- Redis 和 PostgreSQL 的职责边界。
- 为什么 IM 消息必须持久化到关系型数据库。
- ODB 相比手写 SQL 的优势和问题。
- 为什么 migration 不放到服务启动中自动执行。
- 测试数据隔离为什么影响并行 CI。

## 后续扩展

- 更完整的 schema 版本管理。
- 生产迁移回滚策略。
- 消息冷热分层存储。
- Redis pool 压测和容量评估。
- 查询索引优化。
