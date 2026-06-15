# Friend Service 模块说明

## 当前职责

Friend Service 负责好友关系和好友申请生命周期：

- 发送好友申请。
- 接受/拒绝好友申请。
- 查询好友列表。
- 查询待处理申请。
- 维护好友关系持久化。
- gRPC 服务边界和独立 `friend_server` 进程。

用户搜索由 User Service 提供，Friend Service 负责关系本身。

## 对外入口

### Gateway HTTP

```text
POST /api/v1/friends/request
POST /api/v1/friends/respond
GET  /api/v1/friends
GET  /api/v1/friends/pending
```

### gRPC

定义在 `common/proto/friend.proto`：

```text
im.friend_.FriendService.SendRequest
im.friend_.FriendService.RespondToRequest
im.friend_.FriendService.GetFriends
im.friend_.FriendService.GetPendingRequests
```

proto package 使用 `im.friend_`，避免 C++ 关键字 `friend` 与生成代码冲突。

## 核心链路

### 添加好友

```text
Web 搜索用户
-> Gateway GET /api/v1/users/search
-> 展示用户资料卡
-> POST /api/v1/friends/request
-> FriendService 创建 pending 申请
```

### 处理申请

```text
GET /api/v1/friends/pending
-> 展示待处理申请
-> POST /api/v1/friends/respond
-> FriendService 接受或拒绝
-> 接受后形成好友关系
```

## 关键规则

当前服务层覆盖：

- 不能添加自己。
- 目标用户不存在时拒绝。
- 重复申请或反向重复申请拒绝。
- 只有目标用户可以处理申请。
- 非 pending 状态不能重复处理。

## 数据与依赖

- PostgreSQL/ODB：好友申请和好友关系。
- User Service：用于校验目标用户是否存在。
- Gateway：负责 token 身份和 HTTP 响应。

## 面试可讲点

- 好友关系为什么要作为服务独立出来。
- 好友申请和好友关系为什么要区分状态。
- 如何避免重复申请、反向重复申请和自加好友。
- 双向好友关系如何建模。
- package 为什么叫 `friend_`。

## 后续扩展

- 黑名单。
- 好友备注。
- 好友分组。
- 隐私规则。
- 删除好友。
