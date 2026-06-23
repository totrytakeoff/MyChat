# User Service 模块说明

## 当前职责

User Service 负责账号与用户资料相关业务：

- 注册。
- 登录凭证校验。
- 当前用户资料查询。
- 用户资料更新。
- 用户搜索。
- PostgreSQL/ODB 持久化用户信息。
- gRPC 服务边界和独立 `user_server` 进程。

Gateway 负责 token 生成和校验，User Service 负责账号与资料数据本身。

## 对外入口

### Gateway HTTP

```text
POST /api/v1/auth/register
POST /api/v1/auth/login
GET  /api/v1/auth/info
POST /api/v1/auth/profile
GET  /api/v1/users/search
```

### gRPC

定义在 `common/proto/user.proto`：

```text
im.user.UserService.Register
im.user.UserService.Login
im.user.UserService.GetUserInfo
im.user.UserService.SearchUsers
im.user.UserService.UpdateUserInfo
```

服务适配器：

```text
services/user/user_grpc_service.*
```

独立进程：

```text
services/user/user_server
```

## 关键链路

### 注册

```text
Web 客户端
-> Gateway POST /api/v1/auth/register
-> MessageParser / UnifiedMessage
-> GatewayCommandHandlerRegistry
-> GatewayRuntimeRegistry
-> local UserPacketDispatcher 或 remote UserService.ForwardPacket
-> UserPacketDispatcher 解析 user protobuf payload
-> UserService::register_user
-> UserRepository
-> PostgreSQL/ODB
-> Gateway 生成 access_token / refresh_token
-> 返回 profile + tokens
```

### 登录

```text
Gateway POST /api/v1/auth/login
-> Gateway packet 链路转发到 User Service
-> UserService 校验账号密码
-> PasswordHasher 校验 PBKDF2-HMAC-SHA256
-> Gateway 生成 token
```

### 资料更新

```text
Gateway POST /api/v1/auth/profile
-> token 解析得到当前 uid
-> Gateway packet 链路转发到 User Service
-> UserPacketDispatcher 调用 UserService / UserRepository 更新资料
```

资料更新不信任客户端传入 uid，当前用户身份以 token 为准。

## 当前资料字段

用户资料包含：

- uid
- account
- nickname
- avatar
- gender
- signature
- created_at / updated_at / last_login 等服务内部字段

头像当前存储策略：

- 使用现有 `avatar` 文本字段。
- Web 客户端支持 URL 或 base64 data URL。
- 独立上传服务和对象存储是后续扩展，不是当前 MVP 阻塞项。

## 数据与依赖

- PostgreSQL：用户账号和资料持久化。
- ODB：C++ 对象关系映射。
- OpenSSL：密码哈希和随机 UID。
- Redis：token/session 状态由 Gateway auth 侧使用，不属于 User Service 持久化职责。

## 面试可讲点

- 为什么 token 由 Gateway 负责，账号数据由 User Service 负责。
- 为什么服务层不信任客户端传入的 uid。
- PBKDF2-HMAC-SHA256 密码哈希的意义。
- avatar 当前用文本字段的 MVP 取舍，以及生产环境如何演进到对象存储。
- User Service 如何通过 packet dispatcher + ForwardPacket 支持 local 和 remote gRPC 调用。

## 后续扩展

- 账号安全策略，例如登录失败限制、验证码、二次验证。
- 头像上传服务和文件存储。
- 更完整的用户资料字段和隐私设置。
- 用户在线状态与多端设备管理 UI。
