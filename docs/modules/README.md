# 模块文档

本目录用于保存 MyChat 各模块的当前说明文档。

## 当前版模块文档

- [Gateway](gateway/README.md)
- [User Service](user.md)
- [Message Service](message.md)
- [Friend Service](friend.md)
- [Group Service](group.md)
- [Push Service](push.md)
- [存储与缓存](storage.md)
- [Web 验证客户端](web_client.md)

## 历史模块文档

以下目录中的文档是早期使用文档或讲解文档，可能不同程度偏离当前代码，只作为历史参考：

- `gateway/legacy_gateway_docs/`
- `network/legacy_network_docs/`
- `utils/legacy_utils_docs/`
- `redis/redis_legacy.md`
- `database/ODB_开发实践指南.md`
- `database/PostgreSQL_ODB_封装设计文档.md`

如果当前版模块文档和 legacy 文档冲突，以当前版模块文档和代码为准。

## 后续维护规则

- 新模块说明写中文。
- 每份文档都要区分“当前已实现”和“后续扩展”。
- 不继续在 legacy 文档上做补丁式维护；失效内容保留归档，当前说明重写。
