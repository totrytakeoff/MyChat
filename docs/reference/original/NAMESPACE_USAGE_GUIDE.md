# 命名空间使用指南

## 🎯 概述

MyChat项目现在使用规范的命名空间结构来组织代码，避免命名冲突并提高代码可维护性。

## 📋 命名空间结构

```cpp
im::
├── network::         // 网络相关模块
│   ├── WebSocketServer
│   ├── WebSocketSession  
│   ├── ProtobufCodec
│   ├── TCPServer
│   ├── TCPSession
│   └── IOServicePool
└── utils::           // 工具类模块
    ├── LogManager
    ├── ThreadPool
    ├── Singleton<T>
    ├── ConnectionPool<T>
    ├── MemoryPool<T>
    └── ConfigManager
```

## 🔧 使用方法

### 1. 基本使用方式

#### 在你的代码中使用命名空间
```cpp
#include "common/network/websocket_server.hpp"
#include "common/utils/log_manager.hpp"

// 方式1: 使用using声明（推荐）
using namespace im::network;
using namespace im::utils;

int main() {
    // 现在可以直接使用
    WebSocketServer server(...);
    LogManager::SetLogToConsole("app");
    return 0;
}
```

#### 不使用using声明的方式
```cpp
#include "common/network/websocket_server.hpp"
#include "common/utils/log_manager.hpp"

int main() {
    // 使用完整命名空间
    im::network::WebSocketServer server(...);
    im::utils::LogManager::SetLogToConsole("app");
    return 0;
}
```

### 2. 在头文件中的使用

#### 头文件中避免using namespace
```cpp
// my_class.hpp
#include "common/network/websocket_server.hpp"

class MyClass {
private:
    // 使用完整命名空间名称
    std::unique_ptr<im::network::WebSocketServer> server_;
public:
    void init(im::utils::LogManager& logger);
};
```

#### 源文件中可以使用using
```cpp
// my_class.cpp
#include "my_class.hpp"

using namespace im::network;
using namespace im::utils;

void MyClass::init(LogManager& logger) {
    server_ = std::make_unique<WebSocketServer>(...);
}
```

### 3. 跨命名空间引用

#### 在network模块中使用utils模块
```cpp
// websocket_server.cpp
#include "websocket_server.hpp"

namespace im {
namespace network {

using im::utils::LogManager;  // 引用utils模块的LogManager

void WebSocketServer::start() {
    if (LogManager::IsLoggingEnabled("websocket_server")) {
        // 现在可以直接使用LogManager
    }
}

} // namespace network
} // namespace im
```

## 📚 各模块详细说明

### Network 模块 (im::network)

包含所有网络相关的类和功能：

```cpp
using namespace im::network;

// WebSocket相关
WebSocketServer ws_server(...);
WebSocketSession ws_session(...);

// TCP相关
TCPServer tcp_server(...);
TCPSession tcp_session(...);

// 协议相关
ProtobufCodec codec;

// IO服务池
IOServicePool& pool = IOServicePool::GetInstance();
```

### Utils 模块 (im::utils)

包含所有工具类和辅助功能：

```cpp
using namespace im::utils;

// 日志管理
LogManager::SetLogToFile("app", "app.log");

// 线程池
ThreadPool& pool = ThreadPool::GetInstance();

// 单例模式
class MyClass : public Singleton<MyClass> {
    friend class Singleton<MyClass>;
};

// 连接池
ConnectionPool<DatabaseConnection> db_pool;

// 内存池
MemoryPool<MyMessage> msg_pool(1024);

// 配置管理
ConfigManager config("config.json");
```

## 🎯 最佳实践

### 1. 文件组织

#### 在新创建的头文件中
```cpp
#ifndef MY_HEADER_HPP
#define MY_HEADER_HPP

#include <标准库头文件>
#include "其他头文件"

namespace im {
namespace network {  // 或 utils

class MyClass {
    // 类定义
};

} // namespace network
} // namespace im

#endif
```

#### 在新创建的源文件中
```cpp
#include "my_header.hpp"

namespace im {
namespace network {  // 或 utils

// 如果需要使用其他命名空间
using im::utils::LogManager;

// 实现代码

} // namespace network
} // namespace im
```

### 2. 模块间依赖

#### Network模块使用Utils模块（✅ 推荐）
```cpp
namespace im {
namespace network {

using im::utils::LogManager;
using im::utils::ThreadPool;

class WebSocketServer {
    // 使用utils模块的功能
};

} // namespace network
} // namespace im
```

#### Utils模块不应依赖Network模块（❌ 避免）
```cpp
// ❌ 不要这样做
namespace im {
namespace utils {

using im::network::TCPServer;  // 错误：utils不应依赖network

} // namespace utils
} // namespace im
```

### 3. 第三方库命名空间

保持现有的第三方库命名空间使用方式：

```cpp
namespace im {
namespace network {

// Boost命名空间别名
namespace beast = boost::beast;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// 其他第三方库
using json = nlohmann::json;

} // namespace network
} // namespace im
```

### 4. 模板类的使用

```cpp
using namespace im::utils;

// 模板类使用
Singleton<MyClass> my_singleton;
ConnectionPool<DatabaseConn> db_pool;
MemoryPool<Message> msg_pool(1024);
```

## 🚨 常见错误和解决方案

### 错误1: 忘记添加using声明
```cpp
// ❌ 错误
namespace im {
namespace network {

void WebSocketServer::log() {
    LogManager::GetLogger("test");  // 编译错误：LogManager未声明
}

} // namespace network
} // namespace im

// ✅ 正确
namespace im {
namespace network {

using im::utils::LogManager;  // 添加using声明

void WebSocketServer::log() {
    LogManager::GetLogger("test");  // 正确
}

} // namespace network
} // namespace im
```

### 错误2: 在头文件中使用using namespace
```cpp
// ❌ 错误 (header.hpp)
#include "common/utils/log_manager.hpp"
using namespace im::utils;  // 不要在头文件中这样做

class MyClass {
    // ...
};

// ✅ 正确 (header.hpp)
#include "common/utils/log_manager.hpp"

class MyClass {
private:
    im::utils::LogManager* logger_;  // 使用完整命名空间
};
```

### 错误3: 忘记更新包含Singleton的类
```cpp
// ❌ 错误
class IOServicePool : public Singleton<IOServicePool> {
    // 编译错误：Singleton现在在im::utils命名空间中
};

// ✅ 正确
namespace im {
namespace network {

using im::utils::Singleton;  // 添加using声明

class IOServicePool : public Singleton<IOServicePool> {
    // 正确
};

} // namespace network
} // namespace im
```

## 📊 迁移检查清单

在使用新的命名空间结构时，确保：

- [ ] ✅ 所有network类都在`im::network`命名空间中
- [ ] ✅ 所有utils类都在`im::utils`命名空间中  
- [ ] ✅ 跨命名空间引用使用了正确的using声明
- [ ] ✅ 头文件避免使用`using namespace`
- [ ] ✅ 源文件正确添加了命名空间包装
- [ ] ✅ 第三方库命名空间保持不变
- [ ] ✅ 编译通过无错误
- [ ] ✅ 所有测试通过

## 🎉 总结

新的命名空间结构为MyChat项目带来了：

1. **清晰的模块划分** - network和utils职责明确
2. **避免命名冲突** - 不会与第三方库冲突
3. **更好的可维护性** - 明确的依赖关系
4. **工业标准对齐** - 符合现代C++项目规范

遵循本指南，你就能正确使用新的命名空间结构，写出更规范、更易维护的代码！🚀