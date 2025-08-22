# RouterManager - 统一路由管理器

## 🚀 简介

RouterManager 是 MyChat 网关系统的核心路由组件，提供 HTTP 路由解析和服务发现功能。

## ✨ 特性

- 🎯 **HTTP路由解析** - 将HTTP请求映射到命令ID和服务名
- 🔍 **服务发现** - 将服务名映射到具体的服务端点
- ⚡ **一站式路由** - 从HTTP请求直接获取服务端点信息
- 🔄 **配置热更新** - 支持动态重新加载路由配置
- 🧪 **完整测试** - 提供全面的单元测试覆盖

## 🛠️ 快速开始

### 1. 基本使用

```cpp
#include "router_mgr.hpp"

using namespace im::gateway;

// 创建路由管理器
RouterManager router_manager("config.json");

// 执行完整路由
auto result = router_manager.route_request("POST", "/api/v1/auth/login");

if (result && result->is_valid) {
    std::cout << "命令ID: " << result->cmd_id << std::endl;
    std::cout << "服务端点: " << result->endpoint << std::endl;
    std::cout << "超时时间: " << result->timeout_ms << "ms" << std::endl;
}
```

### 2. 配置文件示例

```json
{
    "http_router": {
        "api_prefix": "/api/v1",
        "routes": [
            {
                "path": "/auth/login",
                "cmd_id": 1001,
                "service_name": "user_service"
            }
        ]
    },
    "service_router": {
        "services": [
            {
                "service_name": "user_service",
                "endpoint": "localhost:8001",
                "timeout_ms": 5000,
                "max_connections": 10
            }
        ]
    }
}
```

## 📁 文件结构

```
gateway/router/
├── router_mgr.hpp          # 头文件
├── router.mgr.cpp          # 实现文件
├── config.json             # 示例配置
├── README.md               # 本文档
└── test/                   # 测试文件
    ├── test_router_manager.cpp
    ├── CMakeLists.txt
    └── build_and_test.sh
```

## 🧪 测试

```bash
# 运行所有测试
cd test/router
./build_and_test.sh

# 运行特定测试
./build_and_test.sh -f "*HttpRouter*"
```

## 📚 API 概览

### 核心类

- **`RouterManager`** - 统一路由管理器
- **`HttpRouter`** - HTTP路由解析器  
- **`ServiceRouter`** - 服务发现器

### 主要方法

- **`route_request()`** - 完整路由处理（推荐）
- **`parse_http_route()`** - HTTP路由解析
- **`find_service()`** - 服务发现
- **`reload_config()`** - 配置重新加载
- **`get_stats()`** - 获取统计信息

### 结果结构

- **`CompleteRouteResult`** - 完整路由结果
- **`HttpRouteResult`** - HTTP路由结果
- **`ServiceRouteResult`** - 服务发现结果

## 🔧 依赖

- **C++20** - 编译器支持
- **nlohmann/json** - JSON处理
- **spdlog** - 日志库
- **ConfigManager** - 配置管理
- **LogManager** - 日志管理

## 📖 详细文档

完整的使用文档请参考：[RouterManager使用文档](../../docs/RouterManager使用文档.md)

## 🐛 故障排除

### 常见问题

1. **配置加载失败**
   - 检查配置文件路径和JSON格式
   - 查看日志获取详细错误信息

2. **路由匹配失败**
   - 确认API前缀配置正确
   - 检查路径是否在路由规则中

3. **服务发现失败**
   - 验证服务名称匹配
   - 确认服务端点格式正确

### 调试技巧

```cpp
// 启用详细日志
spdlog::set_level(spdlog::level::debug);

// 查看统计信息
auto stats = router_manager.get_stats();
std::cout << "路由数量: " << stats.http_route_count << std::endl;
std::cout << "服务数量: " << stats.service_count << std::endl;
```

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

MIT License