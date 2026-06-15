# Group Service 模块说明

## 当前职责

Group Service 负责群资料、群成员关系和群消息：

- 创建群。
- 搜索群。
- 查询群资料。
- 加入群。
- 退出群。
- 查询我的群列表。
- 查询群成员。
- 群消息发送和历史查询。
- gRPC 服务边界和独立 `group_server` 进程。

## 对外入口

### Gateway HTTP

```text
POST /api/v1/groups
POST /api/v1/groups/join
POST /api/v1/groups/leave
GET  /api/v1/groups/info
GET  /api/v1/groups/search
GET  /api/v1/groups
GET  /api/v1/groups/members
POST /api/v1/groups/messages/send
GET  /api/v1/groups/messages/history
```

### gRPC

定义在 `common/proto/group.proto`：

```text
im.group.GroupService.CreateGroup
im.group.GroupService.JoinGroup
im.group.GroupService.LeaveGroup
im.group.GroupService.GetGroupInfo
im.group.GroupService.SearchGroups
im.group.GroupService.GroupExists
im.group.GroupService.ListMyGroups
im.group.GroupService.ListMembers
im.group.GroupService.SendGroupMessage
im.group.GroupService.GetGroupMessages
```

proto 中还保留了一些未来群管理消息结构，但当前 MVP 主要实现上述 RPC。

## 核心链路

### 建群

```text
POST /api/v1/groups
-> Gateway 验证 token
-> GroupClient local/remote facade
-> GroupService 创建群资料
-> 创建者加入成员表
```

### 搜索并加群

```text
GET /api/v1/groups/search
-> 展示群资料
-> POST /api/v1/groups/join
-> GroupService 写入成员关系
```

### 群消息

```text
POST /api/v1/groups/messages/send
-> Gateway 验证 token
-> GroupMessageService 持久化群消息
-> GroupService 查询成员
-> PushNotifier 对成员 fanout
-> 默认不向发送者重复推送
```

## 当前 UI 与后端边界

Web 端已经有群资料页、成员展示、加入/退出、群消息验证入口。

以下 UI 形态目前主要是验证/展示层，不应被当作已经完整持久化的后端群设置：

- 群公告。
- 我的群昵称。
- 群备注。
- 置顶。
- 免打扰。
- 通知设置。

## 数据与依赖

- PostgreSQL/ODB：群资料、群成员、群消息。
- Push Service：在线群消息 fanout。
- Gateway：HTTP 入口和 token 身份。

## 面试可讲点

- 群和群成员为什么需要独立建模。
- 群消息 fanout 为什么不向发送者重复推送。
- 成员权限校验应该放在 Group Service，而不是前端。
- 当前群设置 UI 和后端 MVP 能力的边界。
- 后续做群管理时如何扩展角色、邀请、踢人、禁言。

## 后续扩展

- 群主/管理员角色。
- 邀请、踢人、禁言。
- 群公告、群昵称、群备注、通知设置持久化。
- 入群申请审核。
- 大群消息 fanout 优化。
