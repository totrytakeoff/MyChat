# Redis结构设计文档

## 概述

本文档详细描述了MyChat即时通讯系统中使用的Redis数据结构设计，包括认证模块和连接管理模块的Redis键值结构。

## AUTH模块

### 刷新令牌存储
- **键**: `rt:{user_id}:{device_id}:{platform}`
- **值**: 刷新令牌字符串
- **过期时间**: 根据平台配置设置
- **用途**: 存储用户的刷新令牌，用于访问令牌的续期

### 撤销令牌集合
- **键**: `revoked_tokens:{user_id}`
- **值**: 集合类型，包含已撤销的令牌ID
- **过期时间**: 根据令牌过期时间设置
- **用途**: 记录用户已撤销的令牌，防止令牌重用

## ConnectionManager模块

### 用户会话映射
- **键**: `user:sessions:{user_id}`
- **类型**: Hash
- **字段**: `{device_id}:{platform}`
- **值**: JSON序列化的设备会话信息
- **用途**: 存储用户在各个设备上的会话信息

### 设备会话信息结构
```json
{
  "session_id": "会话ID",
  "device_id": "设备ID",
  "platform": "平台标识",
  "connect_time": "连接时间戳"
}
```

### 用户平台设备集合
- **键**: `user:platform:{user_id}`
- **类型**: Set
- **成员**: `{device_id}:{platform}`
- **用途**: 记录用户在各个平台上的设备信息

### 会话到用户映射
- **键**: `session:user:{session_id}`
- **类型**: Hash
- **字段**: `user_id`, `device_id`, `platform`
- **值**: 对应的用户信息
- **用途**: 通过会话ID快速查找用户信息

### 在线用户集合（可选）
- **键**: `online:users`
- **类型**: Set
- **成员**: 用户ID
- **用途**: 维护全局在线用户列表

## 数据清理策略

### 过期策略
1. 会话相关键值对在用户断开连接时主动删除
2. 设置合理的过期时间作为兜底保护
3. 定期清理过期的令牌数据

### 内存优化
1. 使用合理的键命名空间避免冲突
2. 对于大集合数据，考虑分片存储
3. 定期监控Redis内存使用情况

## 使用示例

### 添加用户会话
```
HSET user:sessions:user123 device1:android {"session_id":"session1","device_id":"device1","platform":"android","connect_time":1234567890}
SADD user:platform:user123 device1:android
HSET session:user:session1 user_id user123 device_id device1 platform android
```

### 查询用户会话
```
HGET user:sessions:user123 device1:android
```

### 移除用户会话
```
HDEL user:sessions:user123 device1:android
SREM user:platform:user123 device1:android
DEL session:user:session1
```