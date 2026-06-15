# MyChat 网关实现指导手册

## 一、开始之前的准备工作

### 1.1 开发环境检查
请确保你已经安装以下依赖：
```bash
# 检查必要的库
ls common/network/  # 确认网络基础类已存在
ls common/utils/    # 确认工具类已存在
ls common/proto/    # 确认协议定义已存在
```

### 1.2 理解现有基础组件
在开始编写网关代码之前，建议你先熟悉这些已有的类：
- `WebSocketServer` 和 `WebSocketSession`：WebSocket服务端实现
- `TcpServer` 和 `TcpSession`：HTTP服务端实现  
- `ProtobufCodec`：Protobuf消息编解码器
- `IOServicePool`：IO服务池，用于多线程异步处理
- `ConfigMgr`：配置管理器
- `LogManager`：日志管理器

## 二、第一阶段实现指导：搭建网关框架

### 2.1 创建网关目录结构

首先，让我们在 `gateway/` 目录下创建清晰的代码结构：

```
gateway/
├── src/                    # 源码目录
│   ├── gateway_server.cpp     # 网关主服务类
│   ├── gateway_server.hpp
│   ├── connection_manager.cpp  # 连接管理
│   ├── connection_manager.hpp
│   ├── auth_manager.cpp       # 认证管理
│   ├── auth_manager.hpp
│   ├── route_manager.cpp      # 路由管理
│   ├── route_manager.hpp
│   ├── message_processor.cpp  # 消息处理
│   ├── message_processor.hpp
│   └── main.cpp              # 程序入口
├── config/                 # 配置文件
│   └── gateway_config.json
└── CMakeLists.txt         # 构建配置
```

**任务1**：创建上述目录结构
```bash
mkdir -p gateway/src gateway/config
```

### 2.2 编写网关主服务类框架

**任务2**：创建 `gateway/src/gateway_server.hpp`

这个文件是网关的核心，使用 httplib 库处理 HTTP 请求：

```cpp
#pragma once
#include <memory>
#include <string>
#include <thread>
#include "common/network/websocket_server.hpp"
#include "common/network/IOService_pool.hpp"
#include "common/utils/config_mgr.hpp"
#include "common/utils/log_manager.hpp"
#include "httplib.h"  // httplib 库

// 前向声明你即将创建的类
class ConnectionManager;
class AuthManager;
class RouteManager;
class MessageProcessor;

class GatewayServer {
public:
    GatewayServer();
    ~GatewayServer();
    
    // 核心方法 - 你需要实现这些
    bool Start(const std::string& config_path);
    void Stop();
    
    // 消息处理入口 - 这是关键方法
    void HandleWebSocketMessage(std::shared_ptr<WebSocketSession> session, 
                               const std::string& message);

private:
    // 初始化各个模块 - 分步骤实现
    bool InitializeNetwork();
    bool InitializeManagers();
    bool InitializeHttpRoutes();  // 设置HTTP路由
    
    // HTTP 请求处理函数
    void HandleLoginRequest(const httplib::Request& req, httplib::Response& res);
    void HandleLogoutRequest(const httplib::Request& req, httplib::Response& res);
    void HandleUserInfoRequest(const httplib::Request& req, httplib::Response& res);
    void HandleSendMessageRequest(const httplib::Request& req, httplib::Response& res);
    
    // 网络服务组件
    std::unique_ptr<WebSocketServer> ws_server_;
    std::unique_ptr<httplib::Server> http_server_;  // 使用 httplib
    std::shared_ptr<IOServicePool> io_service_pool_;
    std::thread http_thread_;  // HTTP服务器线程
    
    // 网关核心模块（需要你实现）
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::unique_ptr<AuthManager> auth_mgr_;
    std::unique_ptr<RouteManager> route_mgr_;
    std::unique_ptr<MessageProcessor> msg_processor_;
    
    // 配置和日志
    std::shared_ptr<ConfigMgr> config_mgr_;
    std::shared_ptr<LogManager> log_mgr_;
    
    // 运行状态
    bool is_running_;
    
    // 服务器配置
    std::string http_host_;
    int http_port_;
    int websocket_port_;
};
```

**httplib 的优势**：
1. **简单易用**：只需要包含一个头文件
2. **RESTful 支持**：天然支持 GET、POST、PUT、DELETE 等
3. **JSON 支持**：方便处理 JSON 格式的 HTTP 请求
4. **异步处理**：可以设置线程池处理并发请求

**httplib 使用示例**：

```cpp
// 在 gateway_server.cpp 中的实现示例
bool GatewayServer::InitializeHttpRoutes() {
    if (!http_server_) {
        return false;
    }
    
    // 设置 CORS 支持（如果需要Web客户端访问）
    http_server_->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });
    
    // 用户相关接口
    http_server_->Post("/api/v1/login", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleLoginRequest(req, res);
    });
    
    http_server_->Post("/api/v1/logout", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleLogoutRequest(req, res);
    });
    
    http_server_->Get("/api/v1/user/info", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleUserInfoRequest(req, res);
    });
    
    // 消息相关接口  
    http_server_->Post("/api/v1/message/send", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleSendMessageRequest(req, res);
    });
    
    // 健康检查接口
    http_server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
    
    return true;
}

// HTTP 登录请求处理示例
void GatewayServer::HandleLoginRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        // 1. 解析 JSON 请求体
        nlohmann::json request_json = nlohmann::json::parse(req.body);
        
        std::string username = request_json["username"];
        std::string password = request_json["password"];
        
        // 2. 构造 Protobuf 消息（这里用到 MessageProcessor）
        im::base::IMHeader header;
        header.set_version("1.0");
        header.set_seq(1);  // HTTP请求可以用固定值
        header.set_cmd_id(im::command::CMD_LOGIN);
        header.set_timestamp(GetCurrentTimestamp());
        
        // 3. 构造登录请求载荷
        nlohmann::json payload_json;
        payload_json["username"] = username;
        payload_json["password"] = password;
        
        // 4. 路由到用户服务（这里是简化版，实际通过RouteManager）
        auto response = route_mgr_->CallUserService(header, payload_json.dump());
        
        // 5. 返回 HTTP 响应
        if (response.error_code() == im::base::SUCCESS) {
            res.set_content(response.payload(), "application/json");
        } else {
            res.status = 400;
            nlohmann::json error_json;
            error_json["error"] = response.error_message();
            res.set_content(error_json.dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        res.status = 500;
        nlohmann::json error_json;
        error_json["error"] = "Internal server error";
        res.set_content(error_json.dump(), "application/json");
    }
}
```

**编写指导**：
1. 先写头文件，定义好接口
2. 不要急于实现所有功能，先让程序能编译通过
3. 使用现有的基础组件，不要重新造轮子
4. httplib 可以和 WebSocket 并行运行，各自处理不同类型的客户端

### 2.3 HTTP + WebSocket 双协议架构

**为什么需要两种协议？**

```
┌─────────────────────────────────────────────┐
│              网关双协议设计                   │
├─────────────────────┬───────────────────────┤
│    HTTP 协议        │    WebSocket 协议      │
│                     │                       │
│ • 短连接           │ • 长连接              │
│ • 请求-响应模式     │ • 双向通信            │
│ • 适合简单操作      │ • 适合实时消息        │
│ • 易于调试和测试    │ • 低延迟推送          │
│                     │                       │
│ 典型用例：          │ 典型用例：            │
│ - 用户登录/注册     │ - 实时聊天消息        │
│ - 获取用户信息      │ - 在线状态通知        │
│ - 好友列表查询      │ - 消息推送            │
│ - 文件上传下载      │ - 心跳保活            │
└─────────────────────┴───────────────────────┘
```

**协议选择策略**：

1. **HTTP 接口** (RESTful API)：
   ```bash
   POST /api/v1/login          # 用户登录
   GET  /api/v1/user/info      # 获取用户信息  
   GET  /api/v1/friends        # 获取好友列表
   POST /api/v1/groups         # 创建群组
   ```

2. **WebSocket 接口** (实时通信)：
   ```json
   // 发送消息
   {
     "header": {"cmd_id": 2001, "seq": 123},
     "payload": {"to_user": "user123", "content": "Hello"}
   }
   
   // 接收推送
   {
     "header": {"cmd_id": 5001, "seq": 0},
     "payload": {"from_user": "user456", "content": "Hi there"}
   }
   ```

**在 MessageProcessor 中的处理**：

```cpp
class MessageProcessor {
public:
    // WebSocket 消息处理（Protobuf 格式）
    ParsedMessage ParseWebSocketMessage(const std::string& raw_data);
    
    // HTTP 消息处理（JSON 格式）
    ParsedMessage ParseHttpRequest(const httplib::Request& req, uint32_t cmd_id);
    
    // 统一的响应封装
    std::string PackWebSocketResponse(uint32_t seq, uint32_t cmd_id, 
                                     const im::base::BaseResponse& response);
    
    void PackHttpResponse(const im::base::BaseResponse& response, 
                         httplib::Response& http_res);
};
```

### 2.4 创建连接管理器

**任务3**：创建 `gateway/src/connection_manager.hpp`

连接管理器是网关的重要组件，负责管理所有客户端连接：

```cpp
#pragma once
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <mutex>
#include "common/network/websocket_session.hpp"
#include "common/network/tcp_session.hpp"

// 会话接口基类 - 统一管理不同类型的连接
class IGatewaySession {
public:
    virtual ~IGatewaySession() = default;
    
    // 纯虚函数 - 子类必须实现
    virtual void SendMessage(const std::string& data) = 0;
    virtual void Close() = 0;
    virtual std::string GetSessionId() const = 0;
    
    // 用户绑定信息
    const std::string& GetUserId() const { return user_id_; }
    void SetUserId(const std::string& user_id) { user_id_ = user_id; }
    
    bool IsAuthenticated() const { return authenticated_; }
    void SetAuthenticated(bool auth) { authenticated_ = auth; }

protected:
    std::string user_id_;
    bool authenticated_ = false;
};

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();
    
    // 连接管理
    void AddSession(std::shared_ptr<IGatewaySession> session);
    void RemoveSession(const std::string& session_id);
    
    // 用户绑定
    void BindUserToSession(const std::string& user_id, 
                          const std::string& session_id);
    void UnbindUser(const std::string& user_id);
    
    // 查找连接
    std::shared_ptr<IGatewaySession> FindSessionByUser(const std::string& user_id);
    std::shared_ptr<IGatewaySession> FindSessionById(const std::string& session_id);
    
    // 统计信息
    size_t GetTotalSessions() const;
    size_t GetAuthenticatedSessions() const;

private:
    // 线程安全保护
    mutable std::mutex mutex_;
    
    // 会话存储：session_id -> session
    std::unordered_map<std::string, std::shared_ptr<IGatewaySession>> sessions_;
    
    // 用户映射：user_id -> session_id  
    std::unordered_map<std::string, std::string> user_session_map_;
};
```

**实现提示**：
1. 注意线程安全，所有公共方法都要加锁
2. 使用 weak_ptr 避免循环引用
3. 及时清理断开的连接

### 2.4 创建消息处理器

**任务4**：创建 `gateway/src/message_processor.hpp`

消息处理器负责解析和封装 Protobuf 消息：

```cpp
#pragma once
#include <string>
#include <memory>
#include "common/proto/base.pb.h"
#include "common/network/protobuf_codec.hpp"

class MessageProcessor {
public:
    MessageProcessor();
    ~MessageProcessor();
    
    // 消息解析
    struct ParsedMessage {
        im::base::IMHeader header;
        std::string payload;
        bool valid = false;
    };
    
    ParsedMessage ParseIncomingMessage(const std::string& raw_data);
    
    // 消息封装
    std::string PackResponse(uint32_t seq, 
                           uint32_t cmd_id,
                           const im::base::BaseResponse& response);
    
    // 推送消息封装
    std::string PackPushMessage(const std::string& to_user,
                              uint32_t cmd_id,
                              const std::string& payload);

private:
    // 复用现有的编解码器
    std::unique_ptr<ProtobufCodec> codec_;
    
    // 消息验证
    bool ValidateHeader(const im::base::IMHeader& header);
    bool ValidateCommand(uint32_t cmd_id);
};
```

**MessageProcessor 的使用示例**：

```cpp
// 在 GatewayServer::HandleWebSocketMessage 中使用
void GatewayServer::HandleWebSocketMessage(
    std::shared_ptr<WebSocketSession> session, 
    const std::string& message) {
    
    // 1. 使用 MessageProcessor 解析消息
    auto parsed = msg_processor_->ParseIncomingMessage(message);
    if (!parsed.valid) {
        // 发送错误响应
        SendErrorResponse(session, 0, im::base::INVALID_REQUEST, "Invalid message format");
        return;
    }
    
    // 2. 提取关键信息
    uint32_t cmd_id = parsed.header.cmd_id();
    std::string user_id = parsed.header.from_uid();
    
    // 3. 根据命令类型进行处理
    if (cmd_id == im::command::CMD_LOGIN) {
        // 登录不需要验证Token
        HandleLoginRequest(session, parsed);
    } else {
        // 其他请求需要验证Token
        if (!auth_mgr_->VerifyToken(parsed.header.token(), user_id)) {
            SendErrorResponse(session, parsed.header.seq(), 
                            im::base::AUTH_FAILED, "Token verification failed");
            return;
        }
        
        // 路由到对应服务
        route_mgr_->RouteRequest(session, parsed);
    }
}

// 发送响应时也使用 MessageProcessor
void GatewayServer::SendResponse(std::shared_ptr<Session> session,
                               uint32_t seq, uint32_t cmd_id,
                               const im::base::BaseResponse& response) {
    
    // 使用 MessageProcessor 封装响应
    std::string response_data = msg_processor_->PackResponse(seq, cmd_id, response);
    session->SendMessage(response_data);
}
```

### 2.5 编写程序入口和配置

**任务5**：创建 `gateway/src/main.cpp`

```cpp
#include "gateway_server.hpp"
#include "common/utils/log_manager.hpp"
#include <iostream>
#include <signal.h>

// 全局的网关服务器实例
std::unique_ptr<GatewayServer> g_gateway_server;

// 信号处理函数
void SignalHandler(int signal) {
    std::cout << "Received signal: " << signal << std::endl;
    if (g_gateway_server) {
        g_gateway_server->Stop();
    }
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    // 获取配置文件路径
    std::string config_path = "config/gateway_config.json";
    if (argc > 1) {
        config_path = argv[1];
    }
    
    try {
        // 初始化日志
        LogManager::GetInstance().Init("logs/gateway.log");
        LogManager::GetInstance().Info("Gateway server starting...");
        
        // 创建并启动网关服务器
        g_gateway_server = std::make_unique<GatewayServer>();
        
        if (!g_gateway_server->Start(config_path)) {
            std::cerr << "Failed to start gateway server!" << std::endl;
            return -1;
        }
        
        std::cout << "Gateway server started successfully!" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        
        // 主线程等待
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            // 这里可以添加健康检查或统计信息输出
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Gateway server error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
```

**任务6**：创建配置文件 `gateway/config/gateway_config.json`

```json
{
    "gateway": {
        "name": "MyChat-Gateway",
        "version": "1.0.0",
        "http_host": "0.0.0.0",
        "http_port": 8080,
        "websocket_port": 8081,
        "worker_threads": 4,
        "max_connections": 1000
    },
    "auth": {
        "token_expire_seconds": 86400,
        "secret_key": "your-gateway-secret-key-change-in-production"
    },
    "services": {
        "user_service": {
            "host": "127.0.0.1",
            "port": 9001,
            "timeout_ms": 5000
        },
        "message_service": {
            "host": "127.0.0.1",
            "port": 9002,
            "timeout_ms": 5000
        },
        "friend_service": {
            "host": "127.0.0.1",
            "port": 9003,
            "timeout_ms": 5000
        },
        "group_service": {
            "host": "127.0.0.1",
            "port": 9004,
            "timeout_ms": 5000
        }
    },
    "redis": {
        "host": "127.0.0.1",
        "port": 6379,
        "password": "",
        "db": 0,
        "pool_size": 10
    },
    "logs": {
        "level": "INFO",
        "path": "logs/",
        "max_file_size_mb": 100,
        "max_files": 10
    }
}
```

**任务7**：创建 CMakeLists.txt 文件 `gateway/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyChat-Gateway)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 查找必要的包
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(PkgConfig REQUIRED)

# 设置包含目录
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/common)
include_directories(${Protobuf_INCLUDE_DIRS})

# httplib 支持 (如果没有系统安装，可以作为头文件库使用)
# 下载 httplib.h 放到 third_party/httplib/ 目录下
include_directories(${CMAKE_SOURCE_DIR}/third_party/httplib)

# 收集源文件
file(GLOB_RECURSE GATEWAY_SOURCES
    "src/*.cpp"
    "../common/network/*.cpp"
    "../common/utils/*.cpp"
    "../common/proto/*.cc"
)

# 移除不需要的文件 (如果有的话)
list(FILTER GATEWAY_SOURCES EXCLUDE REGEX ".*test.*")

# 创建可执行文件
add_executable(gateway_server ${GATEWAY_SOURCES})

# 链接库
target_link_libraries(gateway_server
    ${Protobuf_LIBRARIES}
    ${Boost_LIBRARIES}
    pthread
)

# 设置输出目录
set_target_properties(gateway_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 复制配置文件到输出目录
configure_file(
    ${CMAKE_SOURCE_DIR}/config/gateway_config.json
    ${CMAKE_BINARY_DIR}/bin/config/gateway_config.json
    COPYONLY
)

# 创建必要的目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/logs)

# 安装规则 (可选)
install(TARGETS gateway_server DESTINATION bin)
install(FILES config/gateway_config.json DESTINATION bin/config)
```

**依赖安装指导**：

```bash
# Ubuntu/Debian 系统
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libprotobuf-dev \
    protobuf-compiler \
    libboost-all-dev \
    pkg-config

# 下载 httplib (推荐方式)
mkdir -p gateway/third_party/httplib
cd gateway/third_party/httplib
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h

# 或者克隆整个仓库
# git clone https://github.com/yhirose/cpp-httplib.git
```

## 三、第一阶段实现步骤

### 3.1 编码顺序建议

1. **先实现基础框架**
   - 创建所有头文件，定义好接口
   - 实现空的构造函数和析构函数
   - 确保代码能编译通过

2. **实现消息处理器**
   - 这是最基础的功能，先让消息解析工作
   - 测试 Protobuf 消息的序列化和反序列化

3. **实现连接管理器**
   - 管理 WebSocket 和 HTTP 连接
   - 实现会话的添加、删除、查找

4. **实现网关主服务**
   - 集成网络服务器
   - 处理消息接收和分发

### 3.2 调试和测试建议

1. **使用现有工具**
   - 充分利用项目中的 LogManager 记录调试信息
   - 使用 ConfigMgr 管理配置，方便调试

2. **分步骤测试**
   - 先测试 WebSocket 连接建立
   - 再测试消息接收和解析
   - 最后测试完整的请求-响应流程

3. **编写简单的客户端**
   - 可以写个简单的 WebSocket 客户端来测试
   - 发送符合协议格式的消息

## 四、常见问题和解决方案

### 4.1 编译问题
- **找不到头文件**：检查包含路径，确保相对路径正确
- **链接错误**：检查 CMakeLists.txt，确保链接了必要的库

### 4.2 运行时问题
- **端口占用**：检查配置文件中的端口是否被占用
- **连接断开**：添加心跳机制，定期检查连接状态

### 4.3 调试技巧
- **使用 gdb**：`gdb ./gateway_server` 进行调试
- **查看日志**：确保日志输出详细的调试信息
- **分模块测试**：每个模块单独测试，确保功能正确

## 五、下一阶段预告

完成第一阶段后，你将得到一个能够：
1. 接收 WebSocket 和 HTTP 连接
2. 解析 Protobuf 消息
3. 管理客户端会话
4. 基础的消息处理框架

第二阶段我们将实现：
1. Token 认证机制
2. 请求路由到微服务
3. 用户登录流程
4. 消息推送功能

---

现在开始第一阶段的实现吧！记住：**不要试图一次性实现所有功能，先让基础框架跑起来**！

有任何问题随时询问，我会根据你的进度提供具体的代码实现指导。