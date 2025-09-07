# GatewayServer 连接管理流程示例

## 概述

现在的GatewayServer支持完整的连接生命周期管理，包括：
- WebSocket连接建立/断开自动回调
- 传统用户名密码登录
- Token验证直连
- 统一的JSON响应格式

## 1. 连接建立流程

### 方式一：传统登录流程
```
1. 客户端建立WebSocket连接
   → WebSocketServer调用connect_handler
   → GatewayServer::on_websocket_connect()
   → 记录连接，但不绑定用户

2. 客户端发送登录消息 (CMD 1001)
   {
     "cmd_id": 1001,
     "from_uid": "user123",
     "device_id": "device456",
     "platform": "android",
     "body": {
       "username": "user123",
       "password": "password123"
     }
   }
   
3. 服务器验证登录信息
   → 生成Token
   → 自动绑定连接到ConnectionManager
   → 返回响应:
   {
     "code": 200,
     "body": {
       "access_token": "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...",
       "refresh_token": "abc123def456...",
       "message": "Login successful"
     },
     "err_msg": ""
   }
```

### 方式二：Token直连流程
```
1. 客户端建立WebSocket连接
   → 同上

2. 客户端发送Token验证消息 (CMD 1002)
   {
     "cmd_id": 1002,
     "token": "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...",
     "device_id": "device456",
     "platform": "android"
   }
   
3. 服务器验证Token
   → 解析Token获取用户信息
   → 绑定连接到ConnectionManager
   → 返回响应:
   {
     "code": 200,
     "body": {
       "message": "Token verification successful"
     },
     "err_msg": ""
   }
```

## 2. 消息发送流程

```
客户端发送消息 (CMD 2001)
{
  "cmd_id": 2001,
  "from_uid": "user123",
  "to_uid": "user456",
  "token": "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...",
  "body": {
    "content": "Hello, world!",
    "type": "text"
  }
}

服务器处理:
1. 验证Token (MessageProcessor自动处理)
2. 处理消息逻辑
3. 推送给目标用户 (如果在线)
4. 返回响应:
{
  "code": 200,
  "body": {
    "message": "Message sent successfully"
  },
  "err_msg": ""
}
```

## 3. 连接断开流程

```
1. WebSocket连接断开
   → WebSocketServer调用disconnect_handler
   → GatewayServer::on_websocket_disconnect()
   → ConnectionManager::remove_connection()
   → 自动清理用户连接映射
   → 记录日志
```

## 4. 多设备支持

```
同一用户可以在多个设备上同时登录:
- user123@android_device1
- user123@ios_device2
- user123@web_device3

发送消息时会推送到所有在线设备
```

## 5. 错误处理示例

### 无效Token
```json
{
  "code": 401,
  "body": null,
  "err_msg": "Invalid token"
}
```

### 登录失败
```json
{
  "code": 400,
  "body": null,
  "err_msg": "Login failed: Invalid username or password"
}
```

### 会话不存在
```json
{
  "code": 404,
  "body": null,
  "err_msg": "Session not found"
}
```

## 6. 服务器端统计

```cpp
// 获取在线用户数
size_t online_count = gateway->get_online_count();

// 推送消息给特定用户
bool pushed = gateway->push_message_to_user("user123", "系统通知");

// 推送消息给特定设备
bool pushed = gateway->push_message_to_device("user123", "device456", "android", "设备专属消息");

// 获取服务器统计信息
std::string stats = gateway->get_server_stats();
```

## 7. 架构优势

1. **自动连接管理**: WebSocket连接建立/断开自动触发回调
2. **灵活认证方式**: 支持传统登录和Token直连两种方式
3. **统一响应格式**: 使用HttpUtils::buildUnifiedResponse统一构建JSON响应
4. **多设备支持**: 自动处理多设备登录和消息推送
5. **优雅的协程处理**: 使用你设计的协程系统，类似JavaScript async/await
6. **完整的生命周期**: 从连接建立到断开的完整管理
7. **工具类复用**: JSON响应构建逻辑统一在HttpUtils中，便于维护和复用

## 8. HttpUtils工具类的作用

HttpUtils是你的HTTP响应处理工具类，提供了：

### 现有功能：
- `buildResponseString()`: 构建旧格式响应 `{"code": xxx, "body": xxx, "error_message": xxx}`
- `buildResponse()`: 直接构建httplib响应对象
- `statusCodeFromJson()`: 从JSON中提取状态码
- `err_msgFromJson()`: 从JSON中提取错误信息

### 新增功能：
- `buildUnifiedResponse()`: 构建统一格式响应 `{"code": xxx, "body": xxx, "err_msg": xxx}`

### 使用示例：
```cpp
// 成功响应
nlohmann::json body;
body["message"] = "Operation successful";
body["data"] = some_data;
std::string response = HttpUtils::buildUnifiedResponse(200, body);

// 错误响应
std::string response = HttpUtils::buildUnifiedResponse(400, nullptr, "Invalid parameters");

// 在HTTP处理器中使用
HttpUtils::buildResponse(res, response);
```