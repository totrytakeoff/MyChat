# RouterManager 使用文档

## 📋 概述

RouterManager 是 MyChat 网关系统的核心路由管理组件，负责将 HTTP 请求路由到相应的微服务。它整合了 HTTP 路由解析和服务发现功能，提供统一的路由管理接口。

## 🏗️ 架构设计

### 核心组件

```
RouterManager
├── HttpRouter        # HTTP路由解析器
│   ├── 路径匹配      # URL路径到命令ID的映射
│   ├── API前缀处理   # 支持版本化API
│   └── 配置管理      # 动态加载路由规则
└── ServiceRouter     # 服务发现器
    ├── 服务查找      # 服务名到端点的映射
    ├── 连接配置      # 超时和连接数限制
    └── 负载均衡      # 多端点支持(未来扩展)
```

### 数据流

```
HTTP请求 -> HttpRouter -> 命令ID + 服务名 -> ServiceRouter -> 服务端点信息
```

## 🚀 快速开始

### 1. 基本使用

```cpp
#include "gateway/router/router_mgr.hpp"

using namespace im::gateway;

// 创建路由管理器
RouterManager router_manager("config/router_config.json");

// 解析HTTP路由
auto result = router_manager.route_request("POST", "/api/v1/auth/login");

if (result && result->is_valid) {
    std::cout << "命令ID: " << result->cmd_id << std::endl;
    std::cout << "服务名: " << result->service_name << std::endl;
    std::cout << "服务端点: " << result->endpoint << std::endl;
    std::cout << "超时时间: " << result->timeout_ms << "ms" << std::endl;
} else {
    std::cout << "路由失败: " << result->err_msg << std::endl;
}
```

### 2. 分步路由

```cpp
// 第一步：HTTP路由解析
auto http_result = router_manager.parse_http_route("POST", "/api/v1/message/send");
if (http_result && http_result->is_valid) {
    // 第二步：服务发现
    auto service_result = router_manager.find_service(http_result->service_name);
    if (service_result && service_result->is_valid) {
        // 使用服务端点信息
        connect_to_service(service_result->endpoint, service_result->timeout_ms);
    }
}
```

### 3. 配置管理

```cpp
// 重新加载配置
if (router_manager.reload_config()) {
    std::cout << "配置重新加载成功" << std::endl;
} else {
    std::cout << "配置重新加载失败" << std::endl;
}

// 获取统计信息
auto stats = router_manager.get_stats();
std::cout << "HTTP路由数量: " << stats.http_route_count << std::endl;
std::cout << "服务数量: " << stats.service_count << std::endl;
```

## ⚙️ 配置文件格式

### 完整配置示例

```json
{
    "http_router": {
        "api_prefix": "/api/v1",
        "routes": [
            {
                "path": "/auth/login",
                "cmd_id": 1001,
                "service_name": "user_service"
            },
            {
                "path": "/auth/logout",
                "cmd_id": 1002,
                "service_name": "user_service"
            },
            {
                "path": "/message/send",
                "cmd_id": 2001,
                "service_name": "message_service"
            },
            {
                "path": "/friend/list",
                "cmd_id": 3003,
                "service_name": "friend_service"
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
            },
            {
                "service_name": "message_service",
                "endpoint": "localhost:8002",
                "timeout_ms": 3000,
                "max_connections": 20
            },
            {
                "service_name": "friend_service",
                "endpoint": "localhost:8003",
                "timeout_ms": 3000,
                "max_connections": 15
            }
        ]
    }
}
```

### 配置字段说明

#### HTTP路由配置 (`http_router`)

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `api_prefix` | string | 否 | "/api/v1" | API前缀，用于版本管理 |
| `routes` | array | 是 | - | 路由规则数组 |

#### 路由规则 (`routes[]`)

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `path` | string | 是 | 请求路径（不包含API前缀） |
| `cmd_id` | number | 是 | 命令ID，用于标识业务操作 |
| `service_name` | string | 是 | 服务名称，用于服务发现 |

#### 服务配置 (`service_router`)

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `services` | array | 是 | 服务信息数组 |

#### 服务信息 (`services[]`)

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `service_name` | string | 是 | - | 服务名称 |
| `endpoint` | string | 是 | - | 服务端点 (host:port) |
| `timeout_ms` | number | 否 | 3000 | 请求超时时间（毫秒） |
| `max_connections` | number | 否 | 10 | 最大连接数 |

## 📚 API 参考

### RouterManager 类

#### 构造函数

```cpp
explicit RouterManager(const std::string& config_file);
```

**参数:**
- `config_file`: 配置文件路径

**说明:** 创建路由管理器实例并加载配置

#### 核心方法

##### route_request()

```cpp
std::unique_ptr<CompleteRouteResult> route_request(
    const std::string& method, 
    const std::string& path
);
```

**参数:**
- `method`: HTTP请求方法 (GET, POST, PUT, DELETE等)
- `path`: HTTP请求路径

**返回值:** 完整路由结果，包含命令ID和服务端点信息

**说明:** 一站式路由处理，推荐使用此方法

##### parse_http_route()

```cpp
std::unique_ptr<HttpRouteResult> parse_http_route(
    const std::string& method, 
    const std::string& path
);
```

**参数:**
- `method`: HTTP请求方法
- `path`: HTTP请求路径

**返回值:** HTTP路由解析结果

**说明:** 仅进行HTTP路由解析，不包含服务发现

##### find_service()

```cpp
std::unique_ptr<ServiceRouteResult> find_service(
    const std::string& service_name
);
```

**参数:**
- `service_name`: 服务名称

**返回值:** 服务发现结果

**说明:** 根据服务名称查找服务端点信息

##### reload_config()

```cpp
bool reload_config();
```

**返回值:** 成功返回true，失败返回false

**说明:** 重新加载配置文件，支持热更新

##### get_stats()

```cpp
RouterStats get_stats() const;
```

**返回值:** 路由管理器统计信息

**说明:** 获取当前路由和服务数量等统计信息

### 结果结构体

#### CompleteRouteResult

```cpp
struct CompleteRouteResult {
    uint32_t cmd_id;            // 命令ID
    std::string service_name;   // 服务名称
    std::string endpoint;       // 服务端点
    uint32_t timeout_ms;        // 超时时间（毫秒）
    uint32_t max_connections;   // 最大连接数
    bool is_valid;              // 路由是否成功
    std::string err_msg;        // 错误信息
    int http_status_code;       // HTTP状态码
};
```

#### HttpRouteResult

```cpp
struct HttpRouteResult {
    uint32_t cmd_id;            // 命令ID
    std::string service_name;   // 服务名称
    bool is_valid;              // 路由是否有效
    std::string err_msg;        // 错误信息
    int status_code;            // HTTP状态码
};
```

#### ServiceRouteResult

```cpp
struct ServiceRouteResult {
    std::string service_name;   // 服务名称
    std::string endpoint;       // 服务端点
    uint32_t timeout_ms;        // 超时时间（毫秒）
    uint32_t max_connections;   // 最大连接数
    bool is_valid;              // 服务信息是否有效
    std::string err_msg;        // 错误信息
};
```

## 🔧 高级使用

### 1. 错误处理

```cpp
auto result = router_manager.route_request("POST", "/api/v1/unknown/path");

if (!result || !result->is_valid) {
    switch (result->http_status_code) {
        case 404:
            std::cout << "路由未找到: " << result->err_msg << std::endl;
            break;
        case 503:
            std::cout << "服务不可用: " << result->err_msg << std::endl;
            break;
        default:
            std::cout << "路由错误: " << result->err_msg << std::endl;
            break;
    }
}
```

### 2. 性能优化

```cpp
class OptimizedRouter {
private:
    RouterManager router_manager_;
    
public:
    OptimizedRouter(const std::string& config_file) 
        : router_manager_(config_file) {}
    
    // 缓存路由结果
    std::unordered_map<std::string, CompleteRouteResult> route_cache_;
    
    CompleteRouteResult* get_route(const std::string& method, const std::string& path) {
        std::string key = method + ":" + path;
        
        auto it = route_cache_.find(key);
        if (it != route_cache_.end()) {
            return &it->second;  // 返回缓存结果
        }
        
        // 执行路由查找
        auto result = router_manager_.route_request(method, path);
        if (result && result->is_valid) {
            route_cache_[key] = *result;  // 缓存结果
            return &route_cache_[key];
        }
        
        return nullptr;
    }
};
```

### 3. 异步路由

```cpp
#include <future>

class AsyncRouter {
private:
    RouterManager router_manager_;
    
public:
    AsyncRouter(const std::string& config_file) 
        : router_manager_(config_file) {}
    
    std::future<std::unique_ptr<CompleteRouteResult>> 
    route_async(const std::string& method, const std::string& path) {
        return std::async(std::launch::async, [this, method, path]() {
            return router_manager_.route_request(method, path);
        });
    }
};

// 使用示例
AsyncRouter async_router("config.json");
auto future_result = async_router.route_async("POST", "/api/v1/auth/login");

// 执行其他操作...

auto result = future_result.get();  // 获取路由结果
```

### 4. 配置热更新

```cpp
#include <filesystem>
#include <thread>

class HotReloadRouter {
private:
    RouterManager router_manager_;
    std::filesystem::file_time_type last_write_time_;
    std::string config_file_;
    
    void check_config_update() {
        while (true) {
            auto current_time = std::filesystem::last_write_time(config_file_);
            if (current_time != last_write_time_) {
                if (router_manager_.reload_config()) {
                    last_write_time_ = current_time;
                    std::cout << "配置文件已更新并重新加载" << std::endl;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
public:
    HotReloadRouter(const std::string& config_file) 
        : router_manager_(config_file), config_file_(config_file) {
        last_write_time_ = std::filesystem::last_write_time(config_file_);
        
        // 启动配置监控线程
        std::thread([this]() { check_config_update(); }).detach();
    }
    
    auto route_request(const std::string& method, const std::string& path) {
        return router_manager_.route_request(method, path);
    }
};
```

## 🧪 测试

### 单元测试

RouterManager 提供了完整的单元测试套件，位于 `test/router/` 目录：

```bash
# 编译和运行测试
cd test/router
./build_and_test.sh

# 运行特定测试
./build_and_test.sh -f "*HttpRouter*"
```

### 性能测试

```cpp
// 性能测试示例
void benchmark_routing() {
    RouterManager router_manager("config.json");
    
    const int num_requests = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_requests; ++i) {
        auto result = router_manager.route_request("POST", "/api/v1/auth/login");
        assert(result && result->is_valid);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "平均路由时间: " << (duration.count() / num_requests) << " 微秒" << std::endl;
}
```

## 🔍 故障排除

### 常见问题

#### 1. 配置文件加载失败

**问题:** `RouterManager` 构造失败或 `reload_config()` 返回 false

**原因:**
- 配置文件路径错误
- JSON 格式错误
- 缺少必需字段

**解决方案:**
```bash
# 检查配置文件路径
ls -la config/router_config.json

# 验证JSON格式
cat config/router_config.json | python -m json.tool

# 查看日志
tail -f logs/router_manager.log
```

#### 2. 路由匹配失败

**问题:** `route_request()` 返回 `is_valid = false`

**原因:**
- 路径不匹配
- API前缀配置错误
- 路由规则缺失

**解决方案:**
```cpp
// 检查路由配置
auto stats = router_manager.get_stats();
std::cout << "加载的路由数量: " << stats.http_route_count << std::endl;

// 检查具体路径
auto result = router_manager.parse_http_route("POST", "/api/v1/auth/login");
if (!result->is_valid) {
    std::cout << "HTTP路由失败: " << result->err_msg << std::endl;
}
```

#### 3. 服务发现失败

**问题:** HTTP路由成功但服务发现失败

**原因:**
- 服务名称不匹配
- 服务配置缺失
- 端点格式错误

**解决方案:**
```cpp
// 检查服务配置
auto service_result = router_manager.find_service("user_service");
if (!service_result->is_valid) {
    std::cout << "服务发现失败: " << service_result->err_msg << std::endl;
}
```

### 调试技巧

#### 1. 启用详细日志

```cpp
// 在程序开始时设置日志级别
spdlog::set_level(spdlog::level::debug);
```

#### 2. 使用统计信息

```cpp
void print_router_stats(const RouterManager& router) {
    auto stats = router.get_stats();
    std::cout << "=== 路由管理器统计 ===" << std::endl;
    std::cout << "配置文件: " << stats.config_file << std::endl;
    std::cout << "HTTP路由数量: " << stats.http_route_count << std::endl;
    std::cout << "服务数量: " << stats.service_count << std::endl;
}
```

#### 3. 路由测试工具

```cpp
void test_all_routes(RouterManager& router) {
    std::vector<std::pair<std::string, std::string>> test_routes = {
        {"POST", "/api/v1/auth/login"},
        {"POST", "/api/v1/auth/logout"},
        {"POST", "/api/v1/message/send"},
        {"GET", "/api/v1/friend/list"}
    };
    
    for (const auto& [method, path] : test_routes) {
        auto result = router.route_request(method, path);
        std::cout << method << " " << path << " -> ";
        if (result && result->is_valid) {
            std::cout << "成功 (CMD:" << result->cmd_id 
                      << ", 服务:" << result->service_name 
                      << ", 端点:" << result->endpoint << ")" << std::endl;
        } else {
            std::cout << "失败: " << (result ? result->err_msg : "NULL") << std::endl;
        }
    }
}
```

## 📈 最佳实践

### 1. 配置管理

- **版本控制**: 将配置文件纳入版本控制
- **环境隔离**: 为不同环境使用不同的配置文件
- **配置验证**: 在部署前验证配置文件格式
- **备份策略**: 定期备份配置文件

### 2. 性能优化

- **路由缓存**: 对频繁访问的路由进行缓存
- **连接池**: 使用连接池管理服务连接
- **异步处理**: 对于高并发场景使用异步路由
- **监控指标**: 监控路由性能和成功率

### 3. 错误处理

- **优雅降级**: 路由失败时提供默认处理
- **重试机制**: 对临时失败进行重试
- **熔断器**: 防止服务雪崩
- **详细日志**: 记录详细的错误信息

### 4. 安全考虑

- **路径验证**: 验证输入路径格式
- **访问控制**: 基于路由进行权限控制
- **敏感信息**: 避免在日志中记录敏感信息
- **配置加密**: 对敏感配置进行加密存储

## 🔄 版本历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0.0 | 2025-08-21 | 初始版本，支持HTTP路由和服务发现 |

## 📞 支持

如果您在使用 RouterManager 时遇到问题，可以：

1. 查看本文档的故障排除部分
2. 运行单元测试验证功能
3. 检查日志文件获取详细错误信息
4. 提交 Issue 到项目仓库

## 📄 许可证

本项目遵循 MIT 许可证。详见 LICENSE 文件。