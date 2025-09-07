# GatewayServer 安全策略

## 🔐 **安全问题分析与解决方案**

### 原始问题
1. **未认证连接风险**: Token失效后连接仍保持，可能被恶意利用
2. **会话不一致**: HTTP登录与WebSocket连接分离
3. **长连接安全**: 缺乏持续的认证验证机制

### 现在的安全策略

## 🛡️ **多层安全防护**

### 1. 连接时认证（第一层）

#### 有Token的连接
```
客户端连接 → 提取Token → 验证Token
    ↓
✅ 有效: 立即绑定用户，标记为已认证
❌ 无效: 发送错误消息 + 立即关闭连接（100ms延迟确保消息发送）
```

#### 无Token的连接
```
客户端连接 → 发送欢迎消息 → 启动30秒认证倒计时
    ↓
30秒内未认证 → 发送超时消息 + 强制关闭连接
```

### 2. 消息级认证（第二层）

```cpp
// 每个消息处理前的安全检查
if (require_authentication_for_message(msg) && 
    !is_session_authenticated(session_id)) {
    // 发送401错误，拒绝处理
    return "Authentication required";
}
```

#### 认证豁免命令
- `CMD 1001`: 登录命令
- `CMD 1002`: Token验证命令

#### 需要认证的命令
- `CMD 2001`: 发送消息
- `CMD 3001`: 获取好友列表
- 所有其他业务命令

### 3. 会话状态管理（第三层）

```cpp
// 认证状态跟踪
std::unordered_set<std::string> authenticated_sessions_;
std::mutex auth_sessions_mutex_;

// 认证成功时
authenticated_sessions_.insert(session_id);

// 断开连接时
authenticated_sessions_.erase(session_id);
```

## 🚨 **安全时间线**

### 无Token连接流程
```
T+0s:    连接建立 → 发送欢迎消息
T+0-30s: 等待登录/Token验证
T+30s:   强制断开未认证连接
```

### Token连接流程
```
T+0s: 连接建立 → Token验证
  ↓
✅ 立即认证成功 → 正常使用
❌ 立即断开连接 → 安全保护
```

## 🔒 **安全响应格式**

### 认证超时
```json
{
    "code": 408,
    "body": null,
    "err_msg": "Authentication timeout. Connection closed."
}
```

### Token无效
```json
{
    "code": 401,
    "body": null,
    "err_msg": "Token authentication failed. Connection will be closed."
}
```

### 未认证消息
```json
{
    "code": 401,
    "body": null,
    "err_msg": "Authentication required. Please login first."
}
```

### 连接成功（需登录）
```json
{
    "code": 200,
    "body": {
        "message": "Connected successfully. Please login within 30 seconds.",
        "session_id": "session_12345",
        "timeout": 30
    },
    "err_msg": ""
}
```

## 🎯 **安全优势**

### 1. **零信任架构**
- 每个消息都需要验证会话认证状态
- 连接建立不等于认证成功
- 未认证连接有严格的生命周期限制

### 2. **主动防护**
- 无效Token立即断开连接
- 30秒认证超时自动清理
- 实时的认证状态跟踪

### 3. **攻击面最小化**
- 未认证连接只能执行登录相关命令
- 恶意连接无法长期占用服务器资源
- Token泄露风险降到最低

### 4. **优雅降级**
- Token失效时可以通过登录重新认证
- 错误消息明确指导用户操作
- 不会因安全策略影响正常用户体验

## 🛠️ **实现细节**

### 协程超时机制
```cpp
// 使用协程管理器实现非阻塞超时
coro_mgr.schedule([this, session_id, session]() -> Task<void> {
    co_await DelayAwaiter(std::chrono::seconds(30));
    if (!is_session_authenticated(session_id)) {
        session->close();
    }
}());
```

### 线程安全的状态管理
```cpp
// 使用mutex保护认证状态
std::lock_guard<std::mutex> lock(auth_sessions_mutex_);
authenticated_sessions_.insert(session_id);
```

### 延迟关闭机制
```cpp
// 确保错误消息能够发送给客户端
std::thread([session]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    session->close();
}).detach();
```

## 📊 **安全监控**

### 日志记录
- 所有认证失败事件
- 超时断开连接事件
- 未认证消息尝试事件
- IP地址和会话ID关联

### 安全指标
- 认证成功率
- 超时断开连接数量
- 未认证消息拦截数量
- 平均认证时间

## 🔧 **配置建议**

### 超时时间配置
```cpp
// 可以根据业务需求调整
const auto AUTH_TIMEOUT = std::chrono::seconds(30);  // 认证超时
const auto CLOSE_DELAY = std::chrono::milliseconds(100);  // 关闭延迟
```

### 安全等级
- **高安全**: 10秒认证超时，Token失效立即断开
- **标准安全**: 30秒认证超时，Token失效允许重新登录
- **宽松安全**: 60秒认证超时，保持向下兼容

## ✅ **安全检查清单**

- [x] 连接建立时Token自动验证
- [x] 无效Token立即断开连接
- [x] 未认证连接30秒超时
- [x] 消息级认证检查
- [x] 认证状态实时跟踪
- [x] 线程安全的状态管理
- [x] 优雅的错误处理
- [x] 详细的安全日志
- [x] 协程超时机制
- [x] 延迟关闭确保消息发送

现在你的WebSocket服务器已经具备了企业级的安全防护能力！🛡️