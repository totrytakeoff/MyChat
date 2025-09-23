# SignalHandler 信号处理器详细指南

## 概述

`SignalHandler` 是一个线程安全的信号处理器类，提供优雅的进程信号处理功能。它采用单例模式和回调机制，支持多种信号类型的注册和处理。

## 核心特性

- **单例模式**: 全局唯一实例，确保信号处理的一致性
- **回调机制**: 灵活的信号处理回调函数注册
- **线程安全**: 使用互斥锁保护关键操作
- **优雅关闭**: 内置常用关闭信号的处理支持
- **RAII支持**: 提供作用域管理的包装器

## API 参考

### 核心类

```cpp
namespace im::utils {
    class SignalHandler {
    public:
        using SignalCallback = std::function<void(int signal, const std::string& signal_name)>;
        
        static SignalHandler& getInstance();
        
        bool registerSignalHandler(int signal, SignalCallback callback, 
                                  const std::string& signal_name = "");
        bool registerGracefulShutdown(SignalCallback callback);
        
        bool isShutdownRequested() const;
        void waitForShutdown(const std::string& message = "Press Ctrl+C to shutdown...");
        void reset();
        void cleanup();
        
        std::vector<int> getRegisteredSignals() const;
    };
}
```

### 方法详解

#### `getInstance()`
获取单例实例。

```cpp
auto& handler = SignalHandler::getInstance();
```

#### `registerSignalHandler()`
注册指定信号的处理回调。

**参数**:
- `signal`: 信号编号 (如 SIGINT, SIGTERM)
- `callback`: 回调函数
- `signal_name`: 信号名称 (可选，用于日志)

**返回值**: 注册是否成功

```cpp
bool success = handler.registerSignalHandler(SIGUSR1, 
    [](int sig, const std::string& name) {
        std::cout << "Received " << name << std::endl;
    }, "SIGUSR1");
```

#### `registerGracefulShutdown()`
注册常用的优雅关闭信号 (SIGINT, SIGTERM, SIGQUIT)。

```cpp
handler.registerGracefulShutdown([](int sig, const std::string& name) {
    std::cout << "Shutting down gracefully..." << std::endl;
    // 执行清理操作
});
```

#### `isShutdownRequested()`
检查是否收到关闭信号。

```cpp
while (!handler.isShutdownRequested()) {
    // 应用主循环
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

#### `waitForShutdown()`
阻塞等待关闭信号。

```cpp
handler.waitForShutdown("Server is running. Press Ctrl+C to stop...");
```

#### `cleanup()`
清理所有信号处理器，恢复默认行为。

```cpp
handler.cleanup();  // 程序退出前调用
```

### RAII 包装器

```cpp
class ScopedSignalHandler {
public:
    explicit ScopedSignalHandler(SignalCallback callback);
    ~ScopedSignalHandler();
    
    bool isShutdownRequested() const;
    void waitForShutdown(const std::string& message = "") const;
};
```

## 使用示例

### 基本用法

```cpp
#include "common/utils/signal_handler.hpp"
#include <iostream>
#include <thread>

using namespace im::utils;

void gracefulShutdown(int signal, const std::string& signal_name) {
    std::cout << "Received " << signal_name << ", cleaning up..." << std::endl;
    
    // 执行清理操作
    // - 关闭网络连接
    // - 保存数据
    // - 释放资源
    
    std::cout << "Cleanup completed." << std::endl;
}

int main() {
    auto& handler = SignalHandler::getInstance();
    
    // 注册优雅关闭信号
    handler.registerGracefulShutdown(gracefulShutdown);
    
    std::cout << "Application started. Press Ctrl+C to stop." << std::endl;
    
    // 应用主循环
    while (!handler.isShutdownRequested()) {
        // 模拟工作
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Working..." << std::endl;
    }
    
    std::cout << "Application terminated." << std::endl;
    return 0;
}
```

### 使用 RAII 包装器

```cpp
#include "common/utils/signal_handler.hpp"

using namespace im::utils;

int main() {
    // 使用 RAII 包装器，自动清理
    ScopedSignalHandler handler([](int sig, const std::string& name) {
        std::cout << "Received " << name << ", shutting down..." << std::endl;
    });
    
    std::cout << "Server starting..." << std::endl;
    
    // 等待关闭信号
    handler.waitForShutdown("Server is running. Press Ctrl+C to shutdown...");
    
    std::cout << "Server stopped." << std::endl;
    return 0;
} // handler 析构时自动清理
```

### 多信号处理

```cpp
#include "common/utils/signal_handler.hpp"
#include <csignal>

using namespace im::utils;

int main() {
    auto& handler = SignalHandler::getInstance();
    
    // 注册不同信号的处理器
    handler.registerGracefulShutdown([](int sig, const std::string& name) {
        std::cout << "Graceful shutdown requested by " << name << std::endl;
    });
    
    handler.registerSignalHandler(SIGUSR1, [](int sig, const std::string& name) {
        std::cout << "Received " << name << " - Reloading configuration..." << std::endl;
        // 重新加载配置逻辑
    }, "SIGUSR1");
    
    handler.registerSignalHandler(SIGUSR2, [](int sig, const std::string& name) {
        std::cout << "Received " << name << " - Printing statistics..." << std::endl;
        // 打印统计信息
    }, "SIGUSR2");
    
    std::cout << "Multi-signal handler demo started." << std::endl;
    std::cout << "Send SIGUSR1 (kill -USR1 " << getpid() << ") to reload config" << std::endl;
    std::cout << "Send SIGUSR2 (kill -USR2 " << getpid() << ") to print stats" << std::endl;
    std::cout << "Press Ctrl+C to shutdown" << std::endl;
    
    // 主循环
    int counter = 0;
    while (!handler.isShutdownRequested()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Running... (counter: " << ++counter << ")" << std::endl;
    }
    
    handler.cleanup();
    return 0;
}
```

### 在服务器中的应用

```cpp
#include "common/utils/signal_handler.hpp"
#include "your_server.hpp"

using namespace im::utils;

std::shared_ptr<YourServer> g_server;

void serverShutdown(int signal, const std::string& signal_name) {
    std::cout << "Server shutdown requested by " << signal_name << std::endl;
    
    if (g_server) {
        std::cout << "Stopping server..." << std::endl;
        g_server->stop();
        std::cout << "Server stopped successfully." << std::endl;
    }
}

int main() {
    try {
        auto& handler = SignalHandler::getInstance();
        handler.registerGracefulShutdown(serverShutdown);
        
        // 创建并启动服务器
        g_server = std::make_shared<YourServer>();
        g_server->start();
        
        std::cout << "Server started successfully." << std::endl;
        
        // 等待关闭信号
        handler.waitForShutdown("Server is running...");
        
        // 清理
        if (g_server) {
            g_server->stop();
        }
        
        handler.cleanup();
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## 高级用法

### 链式信号处理

```cpp
auto& handler = SignalHandler::getInstance();

// 可以为同一个信号注册多个回调
handler.registerSignalHandler(SIGTERM, callback1);
handler.registerSignalHandler(SIGTERM, callback2);  // 两个回调都会被执行
```

### 条件信号处理

```cpp
handler.registerSignalHandler(SIGINT, [](int sig, const std::string& name) {
    static int count = 0;
    count++;
    
    if (count == 1) {
        std::cout << "First SIGINT received. Press Ctrl+C again to force quit." << std::endl;
    } else {
        std::cout << "Force quit requested." << std::endl;
        exit(1);  // 强制退出
    }
});
```

### 信号状态查询

```cpp
auto& handler = SignalHandler::getInstance();

// 获取已注册的信号列表
auto signals = handler.getRegisteredSignals();
std::cout << "Registered signals: ";
for (int sig : signals) {
    std::cout << sig << " ";
}
std::cout << std::endl;

// 检查关闭状态
if (handler.isShutdownRequested()) {
    std::cout << "Shutdown was requested." << std::endl;
}
```

## 最佳实践

### 1. 信号处理回调设计原则

**DO (推荐)**:
- 保持回调函数简短快速
- 使用原子变量传递状态
- 记录日志信息
- 执行必要的清理操作

```cpp
std::atomic<bool> g_shutdown_flag{false};

handler.registerGracefulShutdown([](int sig, const std::string& name) {
    std::cout << "Shutdown requested by " << name << std::endl;
    g_shutdown_flag.store(true);  // 原子操作
});
```

**DON'T (不推荐)**:
- 在回调中执行复杂的业务逻辑
- 调用非异步信号安全的函数
- 长时间阻塞操作
- 抛出异常

### 2. 资源管理

```cpp
// 推荐：使用RAII
{
    ScopedSignalHandler handler(callback);
    // 应用逻辑
} // 自动清理

// 或者手动管理
auto& handler = SignalHandler::getInstance();
handler.registerGracefulShutdown(callback);
// ... 应用逻辑
handler.cleanup();  // 确保清理
```

### 3. 多线程环境

```cpp
// 信号处理器是线程安全的
std::thread worker([&handler]() {
    while (!handler.isShutdownRequested()) {
        // 工作线程逻辑
    }
});

// 主线程等待信号
handler.waitForShutdown();
worker.join();
```

## 错误处理

### 常见错误及解决方案

#### 1. 重复注册信号
```cpp
// 错误：重复注册会失败
bool success1 = handler.registerSignalHandler(SIGINT, callback1);
bool success2 = handler.registerSignalHandler(SIGINT, callback2);  // 这会成功，添加到回调列表

// 检查注册结果
if (!success1) {
    std::cerr << "Failed to register signal handler" << std::endl;
}
```

#### 2. 信号处理器清理
```cpp
// 确保程序退出前清理
try {
    // 应用逻辑
} catch (...) {
    handler.cleanup();  // 异常情况下也要清理
    throw;
}
handler.cleanup();  // 正常退出时清理
```

#### 3. 回调异常处理
```cpp
handler.registerGracefulShutdown([](int sig, const std::string& name) {
    try {
        // 可能抛出异常的清理代码
        performCleanup();
    } catch (const std::exception& e) {
        std::cerr << "Cleanup error: " << e.what() << std::endl;
        // 不要重新抛出异常
    }
});
```

## 平台兼容性

### Linux/Unix 系统
- 支持所有标准 POSIX 信号
- 推荐使用的信号：SIGINT, SIGTERM, SIGQUIT, SIGUSR1, SIGUSR2

### Windows 系统
- 支持有限的信号集：SIGINT, SIGTERM, SIGABRT
- 某些信号行为可能与 Unix 系统不同

### 信号说明

| 信号 | 编号 | 默认行为 | 用途 |
|------|------|----------|------|
| SIGINT | 2 | 终止 | Ctrl+C 中断 |
| SIGTERM | 15 | 终止 | 正常终止请求 |
| SIGQUIT | 3 | 终止+核心转储 | Ctrl+\ 退出 |
| SIGUSR1 | 10 | 终止 | 用户自定义1 |
| SIGUSR2 | 12 | 终止 | 用户自定义2 |
| SIGHUP | 1 | 终止 | 挂起信号 |

## 性能考虑

### 内存使用
- 单例实例：约 1KB 内存
- 每个回调函数：约 64 字节
- 总体内存开销很小

### CPU 开销
- 信号注册：O(1) 时间复杂度
- 信号处理：取决于回调函数复杂度
- 原子操作开销极小

### 建议
- 保持回调函数简短
- 避免在信号处理中进行 I/O 操作
- 使用原子变量进行状态通信

## 调试技巧

### 1. 启用详细日志
```cpp
handler.registerSignalHandler(SIGINT, [](int sig, const std::string& name) {
    std::cout << "Signal handler called: " << name << " (" << sig << ")" << std::endl;
    // 其他处理逻辑
});
```

### 2. 使用 GDB 调试
```bash
# 设置断点
(gdb) break SignalHandler::signalHandler
(gdb) continue

# 发送信号进行测试
$ kill -INT <pid>
```

### 3. 测试信号处理
```cpp
// 测试代码
#ifdef DEBUG
void testSignalHandler() {
    auto& handler = SignalHandler::getInstance();
    
    handler.registerGracefulShutdown([](int sig, const std::string& name) {
        std::cout << "Test: Received " << name << std::endl;
    });
    
    // 模拟信号
    raise(SIGINT);
    
    assert(handler.isShutdownRequested());
    std::cout << "Signal handler test passed" << std::endl;
}
#endif
```

## 相关文档

- [Common Utils 总览](common-utils-guide.md)
- [CLIParser 指南](cli-parser-guide.md)
- [ConfigManager 指南](config-manager-guide.md)
- [使用示例](../common/utils/example_usage.cpp)

---

*本文档版本: v1.0.0 | 最后更新: 2025-09-18*