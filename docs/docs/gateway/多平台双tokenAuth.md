# 多平台认证系统 (Multi-Platform Authentication System)

## 概述

这是一个基于JWT和Redis的多平台认证系统，支持不同平台的用户认证、令牌管理和权限验证。系统采用双令牌机制（Access Token + Refresh Token）确保安全性和可用性。

## 特性

- ✅ **多平台支持**: Web、Android、iOS、Desktop、MiniApp等
- ✅ **双令牌机制**: Access Token用于API访问，Refresh Token用于令牌刷新
- ✅ **令牌撤销**: 支持令牌黑名单和撤销恢复
- ✅ **设备绑定**: Refresh Token与设备ID强绑定，增强安全性
- ✅ **自动刷新**: 支持令牌自动刷新和轮换
- ✅ **平台策略**: 不同平台可配置不同的令牌策略
- ✅ **Redis集成**: 使用Redis存储令牌元数据和黑名单
- ✅ **线程安全**: 支持并发访问
- ✅ **性能优化**: 连接池管理，高性能令牌操作

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    Multi-Platform Auth                     │
├─────────────────────────────────────────────────────────────┤
│  MultiPlatformAuthManager                                   │
│  ├── Token Generation (JWT)                                 │
│  ├── Token Verification                                     │
│  ├── Token Refresh & Rotation                               │
│  └── Token Revocation                                       │
├─────────────────────────────────────────────────────────────┤
│  PlatformTokenStrategy                                      │
│  ├── Platform-specific Configuration                        │
│  ├── Token Expiration Settings                              │
│  └── Refresh Policies                                       │
├─────────────────────────────────────────────────────────────┤
│  Redis Storage Layer                                        │
│  ├── Refresh Token Metadata                                 │
│  ├── Revoked Token Blacklist                                │
│  └── User Session Management                                │
└─────────────────────────────────────────────────────────────┘
```

## 核心组件

### 1. MultiPlatformAuthManager

主要的认证管理器，提供以下功能：

- `generate_tokens()`: 生成Access Token和Refresh Token
- `verify_access_token()`: 验证Access Token
- `verify_refresh_token()`: 验证Refresh Token
- `refresh_access_token()`: 刷新Access Token
- `revoke_token()` / `unrevoke_token()`: 令牌撤销管理
- `is_token_revoked()`: 检查令牌是否被撤销

### 2. PlatformTokenStrategy

平台策略管理器，支持：

- 平台特定的令牌过期时间
- 自动刷新配置
- 多设备登录策略
- 令牌轮换策略

### 3. Redis集成

使用Redis存储：

- `refresh_tokens`: Refresh Token元数据哈希表
- `revoked_access_tokens`: 被撤销的Access Token集合
- `user:{user_id}:rt`: 用户的Refresh Token集合

## 配置说明

### Redis配置 (`common/database/config.json`)

```json
{
    "redis": {
        "host": "127.0.0.1",
        "port": 6379,
        "password": "",
        "db": 0,
        "pool_size": 10,
        "connect_timeout": 1000,
        "socket_timeout": 1000
    }
}
```

### 平台令牌配置 (`platform_token_config.json`)

```json
{
    "PlatformTokenStrategy": {
        "web": {
            "auto_refresh_enabled": true,
            "background_refresh": true,
            "refresh_precentage": 0.3,
            "max_retry_count": 3,
            "access_token_expire_seconds": 3600,
            "refresh_token_expire_seconds": 604800,
            "enable_multi_device": true
        }
        // ... 其他平台配置
    }
}
```

### 配置参数说明

- `auto_refresh_enabled`: 是否启用自动刷新
- `background_refresh`: 是否支持后台刷新
- `refresh_precentage`: 令牌剩余时间比例低于此值时触发刷新
- `max_retry_count`: 最大重试次数
- `access_token_expire_seconds`: Access Token过期时间（秒）
- `refresh_token_expire_seconds`: Refresh Token过期时间（秒）
- `enable_multi_device`: 是否允许多设备登录

## 使用示例

### 基本用法

```cpp
#include "multi_platform_auth.hpp"
#include "../../common/database/redis_mgr.hpp"

// 初始化Redis
RedisManager::GetInstance().initialize("config.json");

// 创建认证管理器
MultiPlatformAuthManager auth_manager("your_secret_key", "platform_config.json");

// 生成令牌
TokenResult result = auth_manager.generate_tokens(
    "user_123", "john_doe", "device_456", "web");

if (result.success) {
    std::cout << "Access Token: " << result.new_access_token << std::endl;
    std::cout << "Refresh Token: " << result.new_refresh_token << std::endl;
}

// 验证Access Token
UserTokenInfo user_info;
bool valid = auth_manager.verify_access_token(result.new_access_token, user_info);

// 刷新Access Token
std::string device_id = "device_456";
TokenResult refresh_result = auth_manager.refresh_access_token(
    result.new_refresh_token, device_id);
```

### 令牌撤销

```cpp
// 撤销令牌
auth_manager.revoke_token(access_token);

// 检查是否被撤销
bool is_revoked = auth_manager.is_token_revoked(access_token);

// 恢复令牌
auth_manager.unrevoke_token(access_token);
```

## 编译和测试

### 依赖项

- CMake 3.16+
- C++17编译器
- Redis服务器
- redis-plus-plus库
- jwt-cpp库
- nlohmann/json库
- Google Test (用于测试)
- spdlog库
- uuid库

### 编译步骤

```bash
# 进入认证模块目录
cd gateway/auth

# 运行构建和测试脚本
./build_and_test.sh

# 或者手动构建
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# 运行测试
./test_multi_platform_auth

# 运行示例程序
./example_usage
```

### 测试覆盖

测试包括：

- ✅ 令牌生成测试
- ✅ 令牌验证测试
- ✅ 令牌刷新测试
- ✅ 令牌撤销测试
- ✅ 多平台配置测试
- ✅ 边界条件测试
- ✅ 并发安全测试
- ✅ 性能测试

## 安全考虑

### 1. 密钥管理
- 使用强密钥生成Access Token
- 定期轮换密钥
- 密钥存储安全

### 2. 令牌安全
- Access Token短期过期
- Refresh Token长期但可撤销
- 设备绑定验证
- 令牌轮换策略

### 3. 传输安全
- 使用HTTPS传输令牌
- 避免在URL中传递令牌
- 适当的CORS策略

### 4. 存储安全
- Redis访问控制
- 敏感数据加密
- 定期清理过期数据

## 性能特征

### 基准测试结果

- 令牌生成: ~5-20ms/token (取决于平台配置)
- 令牌验证: ~1-5ms/token
- 令牌刷新: ~10-30ms/refresh
- 并发支持: 1000+ QPS

### 优化建议

1. **Redis优化**:
   - 使用Redis集群
   - 配置合适的内存策略
   - 优化连接池大小

2. **应用优化**:
   - 令牌缓存策略
   - 批量操作
   - 异步处理

## 故障排除

### 常见问题

1. **Redis连接失败**
   - 检查Redis服务是否运行
   - 验证连接配置
   - 检查网络连接

2. **令牌验证失败**
   - 检查密钥配置
   - 验证令牌格式
   - 检查令牌是否过期

3. **配置文件错误**
   - 验证JSON格式
   - 检查必需字段
   - 确认文件路径

### 调试信息

启用调试日志：

```cpp
// 设置日志级别
im::utils::LogManager::GetLogger("auth_mgr")->set_level(spdlog::level::debug);
```

## 扩展开发

### 添加新平台

1. 在`global.hpp`中添加新的`PlatformType`
2. 在`get_platform_type()`函数中添加字符串映射
3. 在配置文件中添加平台配置
4. 添加相应的测试用例

### 自定义令牌策略

继承`PlatformTokenStrategy`类并重写相关方法：

```cpp
class CustomTokenStrategy : public PlatformTokenStrategy {
public:
    // 自定义实现
};
```

## 版本历史

- v1.0.0: 初始版本，基本功能实现
- v1.1.0: 添加多平台支持
- v1.2.0: 令牌轮换和撤销功能
- v1.3.0: 性能优化和并发安全

## 许可证

本项目采用MIT许可证。详情请参见LICENSE文件。

## 贡献

欢迎提交Issue和Pull Request来改进本项目。

## 联系方式

如有问题或建议，请联系：myself@example.com