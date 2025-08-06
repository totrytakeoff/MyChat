# WebSocket 连接管理文档

## 概述

WebSocket 连接管理是一个基于 Boost.Beast 和 SSL 的高性能异步 WebSocket 服务器实现。它提供了完整的 WebSocket 连接生命周期管理、消息处理、会话管理和广播功能。

## 核心组件

### WebSocketServer
WebSocket 服务器，负责接受连接、管理会话和消息广播。

### WebSocketSession  
WebSocket 会话，表示单个客户端连接，处理消息收发和连接状态。

## 架构设计

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   WebSocket     │    │   WebSocket     │    │   Message       │
│   Server        │◄──►│   Session       │◄──►│   Handler       │
│                 │    │                 │    │                 │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   SSL Context  │    │   Message Queue │    │   Session ID    │
│   & Acceptor   │    │   & Threading   │    │   Generator     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## API 参考

### WebSocketServer

#### 构造函数
```cpp
WebSocketServer(net::io_context& ioc, 
                ssl::context& ssl_ctx, 
                unsigned short port,
                MessageHandler msg_handler);
```

创建 WebSocket 服务器实例。

**参数**:
- `ioc`: Boost.Asio IO 上下文，用于异步操作
- `ssl_ctx`: SSL 上下文，配置加密参数  
- `port`: 监听端口号
- `msg_handler`: 消息处理回调函数

#### 主要方法

##### start()
```cpp
void start();
```
启动 WebSocket 服务器，开始接受客户端连接。

##### stop()
```cpp  
void stop();
```
停止 WebSocket 服务器，关闭所有连接。

##### broadcast()
```cpp
void broadcast(const std::string& message);
```
向所有连接的客户端广播消息。

**参数**:
- `message`: 要广播的消息内容

##### 会话管理
```cpp
void add_session(SessionPtr session);           // 添加会话
void remove_session(SessionPtr session);        // 移除会话  
void remove_session(const std::string& session_id); // 按ID移除会话
size_t get_session_count() const;               // 获取会话数量
```

### WebSocketSession

#### 构造函数
```cpp
WebSocketSession(tcp::socket socket, 
                 ssl::context& ssl_ctx, 
                 WebSocketServer* server,
                 MessageHandler messageHandler = nullptr,
                 ErrorHandler errorHandler = nullptr);
```

创建 WebSocket 会话实例。

#### 主要方法

##### start()
```cpp
void start();
```
启动会话，执行 SSL 握手和 WebSocket 握手。

##### close()
```cpp
void close();
```
安全关闭会话连接。

##### send()
```cpp
void send(const std::string& message);
```
异步发送消息到客户端。

**参数**:
- `message`: 要发送的消息内容

##### 属性访问
```cpp
const std::string& get_session_id() const;      // 获取会话ID
const WebSocketServer* get_server() const;      // 获取服务器实例
```

##### 处理器设置
```cpp
void set_message_handler(MessageHandler& messageHandler);
void set_error_handler(ErrorHandler& errorHandler);
void set_close_handler(CloseHandler& closeHandler);
```

## 回调函数类型

### MessageHandler
```cpp
using MessageHandler = std::function<void(SessionPtr, beast::flat_buffer&&)>;
```
消息处理回调，当收到客户端消息时调用。

### ErrorHandler
```cpp
using ErrorHandler = std::function<void(SessionPtr, beast::error_code)>;
```
错误处理回调，当连接出现错误时调用。

### CloseHandler
```cpp
using CloseHandler = std::function<void(SessionPtr)>;
```
关闭处理回调，当连接关闭时调用。

## 使用示例

### 基础服务器设置

```cpp
#include "websocket_server.hpp"
#include "websocket_session.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

using namespace im::network;
using namespace im::utils;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

int main() {
    try {
        // 1. 创建 IO 上下文
        net::io_context ioc;
        
        // 2. 配置 SSL 上下文
        ssl::context ssl_ctx{ssl::context::tlsv12};
        ssl_ctx.set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::no_sslv3 |
            ssl::context::single_dh_use);
        
        // 加载 SSL 证书
        ssl_ctx.use_certificate_chain_file("server.crt");
        ssl_ctx.use_private_key_file("server.key", ssl::context::pem);
        
        // 3. 定义消息处理器
        auto message_handler = [](SessionPtr session, beast::flat_buffer&& buffer) {
            std::string message = beast::buffers_to_string(buffer.data());
            std::cout << "收到来自会话 " << session->get_session_id() 
                      << " 的消息: " << message << std::endl;
            
            // 回复消息
            session->send("服务器收到: " + message);
        };
        
        // 4. 创建并启动服务器
        const unsigned short port = 8080;
        WebSocketServer server(ioc, ssl_ctx, port, message_handler);
        server.start();
        
        std::cout << "WebSocket 服务器启动在端口 " << port << std::endl;
        
        // 5. 运行 IO 上下文
        ioc.run();
        
    } catch (std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
    
    return 0;
}
```

### 聊天室服务器示例

```cpp
using namespace im::network;
using namespace im::utils;

class ChatRoomServer {
public:
    ChatRoomServer(net::io_context& ioc, ssl::context& ssl_ctx, unsigned short port)
        : server_(ioc, ssl_ctx, port, 
                 [this](SessionPtr session, beast::flat_buffer&& buffer) {
                     handle_message(session, std::move(buffer));
                 }) {
    }
    
    void start() {
        server_.start();
        std::cout << "聊天室服务器已启动" << std::endl;
    }
    
    void stop() {
        server_.stop();
        std::cout << "聊天室服务器已停止" << std::endl;
    }
    
private:
    void handle_message(SessionPtr session, beast::flat_buffer&& buffer) {
        std::string message = beast::buffers_to_string(buffer.data());
        
        // 解析消息 (简单的 JSON 格式)
        if (message.find("\"type\":\"join\"") != std::string::npos) {
            handle_user_join(session, message);
        } else if (message.find("\"type\":\"message\"") != std::string::npos) {
            handle_chat_message(session, message);
        } else if (message.find("\"type\":\"leave\"") != std::string::npos) {
            handle_user_leave(session);
        }
    }
    
    void handle_user_join(SessionPtr session, const std::string& message) {
        // 提取用户名 (简化处理)
        std::string username = extract_username(message);
        user_sessions_[session->get_session_id()] = username;
        
        // 通知所有用户
        std::string notification = create_notification(username + " 加入了聊天室");
        server_.broadcast(notification);
        
        // 发送欢迎消息
        session->send(create_welcome_message(username));
    }
    
    void handle_chat_message(SessionPtr session, const std::string& message) {
        std::string session_id = session->get_session_id();
        if (user_sessions_.find(session_id) != user_sessions_.end()) {
            std::string username = user_sessions_[session_id];
            std::string content = extract_message_content(message);
            
            // 广播聊天消息
            std::string chat_message = create_chat_message(username, content);
            server_.broadcast(chat_message);
        }
    }
    
    void handle_user_leave(SessionPtr session) {
        std::string session_id = session->get_session_id();
        if (user_sessions_.find(session_id) != user_sessions_.end()) {
            std::string username = user_sessions_[session_id];
            user_sessions_.erase(session_id);
            
            // 通知其他用户
            std::string notification = create_notification(username + " 离开了聊天室");
            server_.broadcast(notification);
        }
    }
    
    // 辅助方法
    std::string extract_username(const std::string& message) {
        // JSON 解析逻辑 (简化)
        return "用户" + std::to_string(std::rand() % 1000);
    }
    
    std::string extract_message_content(const std::string& message) {
        // JSON 解析逻辑 (简化)
        return message;
    }
    
    std::string create_notification(const std::string& content) {
        return "{\"type\":\"notification\",\"content\":\"" + content + "\"}";
    }
    
    std::string create_welcome_message(const std::string& username) {
        return "{\"type\":\"welcome\",\"username\":\"" + username + "\"}";
    }
    
    std::string create_chat_message(const std::string& username, const std::string& content) {
        return "{\"type\":\"chat\",\"username\":\"" + username + "\",\"content\":\"" + content + "\"}";
    }
    
private:
    WebSocketServer server_;
    std::unordered_map<std::string, std::string> user_sessions_; // session_id -> username
};

// 使用示例
int main() {
    net::io_context ioc;
    ssl::context ssl_ctx{ssl::context::tlsv12};
    
    // SSL 配置...
    
    ChatRoomServer chat_server(ioc, ssl_ctx, 8080);
    chat_server.start();
    
    ioc.run();
    return 0;
}
```

### 自定义消息处理示例

```cpp
class GameServer {
private:
    WebSocketServer server_;
    std::unordered_map<std::string, PlayerInfo> players_;
    
public:
    GameServer(net::io_context& ioc, ssl::context& ssl_ctx, unsigned short port)
        : server_(ioc, ssl_ctx, port, 
                 [this](SessionPtr session, beast::flat_buffer&& buffer) {
                     process_game_message(session, std::move(buffer));
                 }) {
        
        // 设置定时器进行游戏循环
        setup_game_loop();
    }
    
private:
    void process_game_message(SessionPtr session, beast::flat_buffer&& buffer) {
        std::string message = beast::buffers_to_string(buffer.data());
        std::string session_id = session->get_session_id();
        
        // 根据消息类型处理
        if (message.find("\"action\":\"move\"") != std::string::npos) {
            handle_player_move(session_id, message);
        } else if (message.find("\"action\":\"attack\"") != std::string::npos) {
            handle_player_attack(session_id, message);
        } else if (message.find("\"action\":\"join\"") != std::string::npos) {
            handle_player_join(session_id, session);
        }
    }
    
    void handle_player_move(const std::string& session_id, const std::string& message) {
        // 处理玩家移动
        if (players_.find(session_id) != players_.end()) {
            // 更新玩家位置
            update_player_position(session_id, message);
            
            // 广播位置更新
            broadcast_player_update(session_id);
        }
    }
    
    void handle_player_attack(const std::string& session_id, const std::string& message) {
        // 处理玩家攻击
        process_attack(session_id, message);
    }
    
    void handle_player_join(const std::string& session_id, SessionPtr session) {
        // 新玩家加入
        PlayerInfo player;
        player.session = session;
        player.x = 0;
        player.y = 0;
        player.health = 100;
        
        players_[session_id] = player;
        
        // 发送游戏状态
        send_game_state(session);
    }
    
    void broadcast_game_update() {
        std::string update = create_game_update_message();
        server_.broadcast(update);
    }
    
    void setup_game_loop() {
        // 设置游戏循环定时器
        // 每秒更新游戏状态
    }
};
```

## 最佳实践

### 1. 错误处理
```cpp
auto error_handler = [](SessionPtr session, beast::error_code ec) {
    if (ec == websocket::error::closed || 
        ec == net::error::eof || 
        ec == net::error::connection_reset) {
        // 正常关闭，清理资源
        cleanup_session(session);
    } else {
        // 异常错误，记录日志
        log_error("会话错误", session->get_session_id(), ec.message());
    }
};
```

### 2. 消息队列管理
```cpp
// 避免消息队列过大导致内存问题
class MessageLimiter {
    static constexpr size_t MAX_QUEUE_SIZE = 1000;
    
public:
    bool should_accept_message(SessionPtr session) {
        return session->get_queue_size() < MAX_QUEUE_SIZE;
    }
};
```

### 3. 连接管理
```cpp
// 定期清理无效连接
void cleanup_inactive_sessions() {
    auto now = std::chrono::steady_clock::now();
    
    std::vector<std::string> to_remove;
    for (const auto& [session_id, last_activity] : session_activity_) {
        if (now - last_activity > std::chrono::minutes(5)) {
            to_remove.push_back(session_id);
        }
    }
    
    for (const auto& session_id : to_remove) {
        server_.remove_session(session_id);
        session_activity_.erase(session_id);
    }
}
```

### 4. 性能优化
```cpp
// 使用对象池减少内存分配
class MessagePool {
public:
    std::string acquire() {
        if (!pool_.empty()) {
            std::string msg = std::move(pool_.back());
            pool_.pop_back();
            return msg;
        }
        return std::string{};
    }
    
    void release(std::string&& msg) {
        msg.clear();
        if (pool_.size() < MAX_POOL_SIZE) {
            pool_.push_back(std::move(msg));
        }
    }
    
private:
    std::vector<std::string> pool_;
    static constexpr size_t MAX_POOL_SIZE = 100;
};
```

## 配置选项

### SSL 配置
```cpp
// 生产环境 SSL 配置
ssl::context ssl_ctx{ssl::context::tlsv12};
ssl_ctx.set_options(
    ssl::context::default_workarounds |
    ssl::context::no_sslv2 |
    ssl::context::no_sslv3 |
    ssl::context::single_dh_use);

// 加载证书
ssl_ctx.use_certificate_chain_file("path/to/cert.pem");
ssl_ctx.use_private_key_file("path/to/private.key", ssl::context::pem);

// 设置密码回调 (如果私钥有密码)
ssl_ctx.set_password_callback([](std::size_t, ssl::context::password_purpose) {
    return "your_private_key_password";
});
```

### 性能调优
```cpp
// IO 上下文线程池
const size_t thread_count = std::thread::hardware_concurrency();
std::vector<std::thread> threads;

for (size_t i = 0; i < thread_count; ++i) {
    threads.emplace_back([&ioc] { ioc.run(); });
}

// 等待所有线程完成
for (auto& t : threads) {
    t.join();
}
```

## 故障排除

### 常见问题

#### 1. SSL 握手失败
**症状**: 客户端连接超时或握手错误  
**解决**: 检查证书配置和客户端 SSL 设置

#### 2. 消息发送失败
**症状**: send() 调用后消息未收到  
**解决**: 检查连接状态和消息队列

#### 3. 内存泄漏
**症状**: 长时间运行后内存持续增长  
**解决**: 确保正确清理会话和消息缓冲区

### 调试技巧

#### 启用详细日志
```cpp
// 在编译时定义调试宏
#define WEBSOCKET_DEBUG 1

// 在代码中使用条件日志
#ifdef WEBSOCKET_DEBUG
    std::cout << "会话 " << session_id << " 发送消息: " << message << std::endl;
#endif
```

#### 连接状态监控
```cpp
class ConnectionMonitor {
public:
    void log_connection(const std::string& session_id) {
        connections_[session_id] = std::chrono::steady_clock::now();
    }
    
    void log_disconnection(const std::string& session_id) {
        auto it = connections_.find(session_id);
        if (it != connections_.end()) {
            auto duration = std::chrono::steady_clock::now() - it->second;
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
            std::cout << "会话 " << session_id << " 连接时长: " 
                      << seconds.count() << " 秒" << std::endl;
            connections_.erase(it);
        }
    }
    
private:
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> connections_;
};
```

## 测试

项目包含完整的测试套件，位于 `test/network/test_websocket.cpp`：

```bash
# 编译测试
cd test/network/build
cmake ..
make websocket_test

# 运行所有测试
./websocket_test

# 运行特定测试
./websocket_test --gtest_filter="WebSocketServerTest.*"
```

### 测试覆盖
- ✅ 服务器启动和停止
- ✅ 会话管理和生命周期  
- ✅ 消息发送和广播
- ✅ 并发安全性
- ✅ 错误处理
- ✅ 性能压力测试

## 设计原则

1. **异步优先**: 所有网络操作都是异步的，避免阻塞
2. **RAII 管理**: 自动资源管理，防止泄漏
3. **类型安全**: 强类型接口，编译时错误检查
4. **可扩展性**: 支持自定义消息处理和错误处理
5. **高性能**: 零拷贝消息传递，对象池优化

## 版本兼容性

- **Boost**: 1.75.0+
- **OpenSSL**: 1.1.1+  
- **C++**: C++17+
- **编译器**: GCC 9+, Clang 10+, MSVC 2019+

---

*更多详细信息和最新更新，请参考项目源代码和测试用例。*