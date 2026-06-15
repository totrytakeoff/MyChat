# Common Utils 通用工具类使用指南

## 概述

本文档详细介绍了 MyChat 项目中的通用工具类库，这些类位于 `common/utils` 目录下，为整个项目提供基础的通用功能。

## 工具类概览

| 类名 | 文件 | 功能描述 | 特性 |
|------|------|----------|------|
| `SignalHandler` | `signal_handler.hpp` | 信号处理器 | 回调机制、优雅关闭、线程安全 |
| `CLIParser` | `cli_parser.hpp` | 命令行参数解析 | 回调机制、自动帮助、类型验证 |
| `ConfigManager` | `config_mgr.hpp` | 配置管理器 | 环境变量、.env文件、JSON配置 |

## 设计理念

### 1. 回调驱动设计
所有工具类都采用回调机制，提供最大的灵活性：
- 用户可以注册自定义处理函数
- 支持链式处理和多重回调
- 错误处理通过返回值反馈

### 2. 类型安全
使用模板和类型检查确保类型安全：
- 编译期类型检查
- 自动类型转换
- 默认值支持

### 3. 线程安全
关键操作使用互斥锁保护：
- 信号处理器的回调注册
- 配置的并发访问
- 原子操作用于状态管理

### 4. 易用性
提供简洁的API和丰富的便利方法：
- RAII资源管理
- 智能默认值
- 详细的错误信息

## 快速开始

### 基本使用示例

```cpp
#include "common/utils/signal_handler.hpp"
#include "common/utils/cli_parser.hpp"
#include "common/utils/config_mgr.hpp"

using namespace im::utils;

int main(int argc, char* argv[]) {
    // 1. 信号处理
    auto& signal_handler = SignalHandler::getInstance();
    signal_handler.registerGracefulShutdown([](int signal, const std::string& name) {
        std::cout << "Received " << name << ", shutting down..." << std::endl;
        // 执行清理操作
    });
    
    // 2. 命令行解析
    CLIParser parser("myapp", "My Application");
    parser.addArgument("port", 'p', ArgumentType::INTEGER, false, "Port number", "8080", "Network",
        [](const std::string& value) -> bool {
            int port = std::stoi(value);
            std::cout << "Port set to: " << port << std::endl;
            return true;
        });
    
    auto result = parser.parse(argc, argv);
    if (!result.success) {
        std::cerr << result.error_message << std::endl;
        return 1;
    }
    
    // 3. 配置管理
    ConfigManager config("app.json", true, "APP");  // 加载配置文件和环境变量
    config.loadEnvFile(".env");  // 加载.env文件
    
    std::string host = config.getWithEnv<std::string>("host", "APP_HOST", "localhost");
    int port = config.getWithEnv<int>("port", "APP_PORT", 8080);
    
    // 应用主循环
    while (!signal_handler.isShutdownRequested()) {
        // 应用逻辑
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

## 集成指南

### 在现有项目中集成

1. **包含头文件**
```cpp
#include "common/utils/signal_handler.hpp"
#include "common/utils/cli_parser.hpp"
#include "common/utils/config_mgr.hpp"
```

2. **链接库**
在 CMakeLists.txt 中：
```cmake
target_link_libraries(your_target
    PRIVATE
        im::utils
)
```

3. **命名空间**
```cpp
using namespace im::utils;
// 或者使用完整命名空间
im::utils::SignalHandler& handler = im::utils::SignalHandler::getInstance();
```

### 最佳实践

#### 1. 信号处理最佳实践
- 在主函数开始时注册信号处理器
- 使用RAII包装器自动清理
- 避免在信号处理回调中执行复杂操作

```cpp
// 推荐：使用RAII包装器
{
    ScopedSignalHandler handler([](int sig, const std::string& name) {
        // 简单的清理操作
        g_shutdown_flag = true;
    });
    
    // 应用逻辑
    while (!handler.isShutdownRequested()) {
        // ...
    }
} // 自动清理
```

#### 2. 命令行解析最佳实践
- 使用参数分组提高帮助信息可读性
- 提供合理的默认值
- 在回调中进行参数验证

```cpp
CLIParser parser("app", "Description");

// 网络配置组
parser.addArgument("host", 'H', ArgumentType::STRING, false, 
                  "Host address", "0.0.0.0", "Network", callback);
parser.addArgument("port", 'p', ArgumentType::INTEGER, false,
                  "Port number", "8080", "Network", callback);

// 日志配置组  
parser.addArgument("log-level", 'l', ArgumentType::STRING, false,
                  "Log level", "info", "Logging", callback);
```

#### 3. 配置管理最佳实践
- 优先级：命令行参数 > 环境变量 > 配置文件 > 默认值
- 使用类型安全的getter方法
- 预定义环境变量列表

```cpp
// 推荐的配置加载顺序
ConfigManager config("app.json", true, "APP");
config.loadEnvFile(".env", true);  // 覆盖配置文件

// 预定义环境变量列表
std::vector<std::string> app_vars = {"HOST", "PORT", "LOG_LEVEL", "DEBUG"};
config.loadEnvironmentVariables(app_vars, "MYAPP");

// 类型安全访问
std::string host = config.getWithEnv<std::string>("host", "MYAPP_HOST", "localhost");
```

## 错误处理

### 常见错误及解决方案

#### 1. 信号处理器错误
- **问题**: 信号处理器重复注册
- **解决**: 检查返回值，使用cleanup()清理

#### 2. 命令行解析错误  
- **问题**: 参数验证失败
- **解决**: 在回调中返回false，检查ParseResult

#### 3. 配置管理错误
- **问题**: 类型转换失败
- **解决**: 使用try-catch，提供默认值

## 性能考虑

### 内存使用
- 信号处理器使用单例模式，内存开销小
- 命令行解析器在解析完成后可以销毁
- 配置管理器缓存配置数据，避免重复读取

### CPU开销
- 信号处理使用原子操作，开销极小
- 配置访问使用哈希表，O(1)时间复杂度
- 类型转换在编译期优化

## 扩展开发

### 添加新的参数类型
```cpp
// 在ArgumentType枚举中添加新类型
enum class ArgumentType {
    // ... 现有类型
    CUSTOM_TYPE
};

// 在validateArgumentValue中添加验证逻辑
```

### 添加新的配置源
```cpp
class ConfigManager {
    // 添加新的加载方法
    bool loadFromDatabase(const std::string& connection_string);
    bool loadFromRemote(const std::string& url);
};
```

## 调试和测试

### 调试技巧
1. 使用详细日志模式
2. 检查回调返回值
3. 使用gdb调试信号处理

### 测试建议
1. 单元测试每个工具类
2. 集成测试验证组合使用
3. 压力测试信号处理性能

## 相关文档

- [SignalHandler 详细文档](signal-handler-guide.md)
- [CLIParser 详细文档](cli-parser-guide.md)  
- [ConfigManager 详细文档](config-manager-guide.md)
- [使用示例](../common/utils/example_usage.cpp)
- [Gateway 集成示例](../gateway/main_example.cpp)

## 更新日志

### v1.0.0 (2025-09-18)
- 初始版本发布
- 实现SignalHandler、CLIParser、ConfigManager
- 添加回调机制支持
- 集成环境变量和.env文件支持

---

*本文档持续更新中，如有问题请提交Issue或联系开发团队。*