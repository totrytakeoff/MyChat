# 分布式服务标识管理器使用指南

## 🎯 设计目标

为分布式即时通讯系统中的每个服务实例提供唯一标识，支持：
- 服务发现和注册
- 分布式追踪
- 负载均衡
- 监控和日志聚合

## 🚀 快速开始

### 1. 基本使用

```cpp
#include "common/utils/service_identity.hpp"

// 在服务启动时初始化
using namespace im::utils;

// 方法1: 从环境变量自动初始化
ServiceIdentityManager::getInstance().initializeFromEnv("gateway");

// 方法2: 手动配置初始化
ServiceIdentityConfig config;
config.service_name = "gateway";
config.cluster_id = "production";
config.region = "us-west-1";
ServiceIdentityManager::getInstance().initialize(config);

// 使用便捷函数
std::string device_id = ServiceId::getDeviceId();
std::string platform = ServiceId::getPlatformInfo();
```

### 2. 环境变量配置

```bash
# 基本配置
export SERVICE_NAME=gateway
export CLUSTER_ID=production
export REGION=us-west-1
export INSTANCE_ID=gateway-001  # 可选，不设置则自动生成

# 启动服务
./gateway_server
```

### 3. 在protobuf响应中使用

```cpp
// 构建响应时使用服务标识
base::IMHeader response_header = ProtobufCodec::returnHeaderBuilder(
    request_header, 
    ServiceId::getDeviceId(),      // 当前服务的设备ID
    ServiceId::getPlatformInfo()   // 当前服务的平台信息
);
```

## 📋 配置选项

### 环境变量

| 变量名 | 默认值 | 说明 |
|--------|--------|------|
| SERVICE_NAME | - | 服务名称（必填） |
| CLUSTER_ID | default | 集群标识 |
| REGION | local | 区域/数据中心 |
| INSTANCE_ID | auto | 实例ID（自动生成或手动指定） |

### 生成的标识格式

```
设备ID格式：{service_name}-{cluster_id}-{instance_id}
示例：gateway-production-myhost-12345-1672531200000-1234

平台信息格式：{platform}-{region}-server
示例：linux-x64-us-west-1-server

完整标识：{service_name}/{cluster_id}/{instance_id}@{region}
示例：gateway/production/myhost-12345-1672531200000-1234@us-west-1
```

## 🔧 在各服务中的集成

### Gateway服务
```cpp
// gateway_server.cpp
#include "common/utils/service_identity.hpp"

GatewayServer::GatewayServer(...) {
    // 初始化服务标识
    if (!ServiceIdentityManager::getInstance().initializeFromEnv("gateway")) {
        throw std::runtime_error("Failed to initialize gateway service identity");
    }
}
```

### Message服务
```cpp
// message_service.cpp
#include "common/utils/service_identity.hpp"

MessageService::MessageService(...) {
    ServiceIdentityConfig config;
    config.service_name = "message-service";
    config.cluster_id = "message-cluster";
    ServiceIdentityManager::getInstance().initialize(config);
}
```

### User服务
```cpp
// user_service.cpp
#include "common/utils/service_identity.hpp"

UserService::UserService(...) {
    ServiceIdentityManager::getInstance().initializeFromEnv("user-service");
}
```

## 🌐 Docker部署配置

```yaml
# docker-compose.yml
version: '3.8'
services:
  gateway-1:
    image: mychat/gateway:latest
    environment:
      - SERVICE_NAME=gateway
      - CLUSTER_ID=main-cluster
      - REGION=us-west-1
      - INSTANCE_ID=gateway-001
    
  gateway-2:
    image: mychat/gateway:latest
    environment:
      - SERVICE_NAME=gateway
      - CLUSTER_ID=main-cluster
      - REGION=us-west-1
      - INSTANCE_ID=gateway-002
      
  message-service:
    image: mychat/message-service:latest
    environment:
      - SERVICE_NAME=message-service
      - CLUSTER_ID=message-cluster
      - REGION=us-west-1
```

## 🔍 监控和日志

```cpp
// 获取服务运行时信息
auto& identity_mgr = ServiceIdentityManager::getInstance();
auto uptime = identity_mgr.getUptimeSeconds();
auto full_identity = ServiceId::getFullIdentity();

// 日志中包含服务标识
logger->info("Service {} processing request, uptime: {}s", 
             full_identity, uptime);

// 更新运行时信息
std::unordered_map<std::string, std::string> runtime_info = {
    {"version", "1.2.3"},
    {"build_time", "2024-01-01"},
    {"active_connections", std::to_string(conn_count)}
};
identity_mgr.updateServiceInfo(runtime_info);
```

## 🎯 最佳实践

1. **统一初始化**: 在服务的main函数或构造函数中尽早初始化
2. **环境变量优先**: 优先使用环境变量配置，便于容器化部署
3. **服务发现集成**: 将标识信息注册到服务发现中心
4. **监控集成**: 在监控指标中包含服务标识信息
5. **日志标准化**: 在日志中包含服务标识，便于分布式追踪

## ⚠️ 注意事项

- 在分布式环境中，确保每个实例的标识都是唯一的
- 服务标识应该在服务启动时就确定，运行过程中不应该改变
- 在容器环境中，建议通过环境变量或配置文件指定标识信息