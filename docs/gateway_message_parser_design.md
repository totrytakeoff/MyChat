# Gateway消息解析器设计规划

## 1. 整体设计思路

### 1.1 设计目标
消息解析器作为Gateway的核心组件，负责统一处理来自客户端的HTTP请求和WebSocket消息，并将其转换为内部标准格式进行路由和处理。

### 1.2 核心职责
1. **协议适配**：统一HTTP和WebSocket两种协议的消息格式
2. **消息解析**：解析客户端消息并验证格式合法性
3. **路由预处理**：为Router模块准备标准化的消息格式
4. **错误处理**：统一的错误响应机制

### 1.3 设计原则
- **协议无关性**：上层业务逻辑不关心消息来源协议
- **可扩展性**：方便后续支持新的协议类型
- **高性能**：最小化消息转换开销
- **安全性**：严格的消息验证和权限检查

## 2. 架构设计

### 2.1 整体架构图

```
客户端
  ↓
┌─────────────────────┐
│   GatewayServer     │
│  ┌───────────────┐  │
│  │   HTTP请求    │  │ ← httplib
│  └───────────────┘  │
│  ┌───────────────┐  │
│  │ WebSocket消息 │  │ ← boost::beast
│  └───────────────┘  │
└─────────────────────┘
  ↓
┌─────────────────────┐
│  MessageParser      │  ← 本次要实现的核心
│  ┌───────────────┐  │
│  │  HTTP Parser  │  │
│  └───────────────┘  │
│  ┌───────────────┐  │
│  │WebSocket Parser│ │
│  └───────────────┘  │
│  ┌───────────────┐  │
│  │Message Factory│  │
│  └───────────────┘  │
└─────────────────────┘
  ↓
┌─────────────────────┐
│   RouteManager      │  ← 现有的路由组件
└─────────────────────┘
```

### 2.2 核心组件设计

#### 2.2.1 MessageParser (主解析器)
```cpp
class MessageParser {
public:
    // 解析HTTP请求
    std::unique_ptr<InternalMessage> parse_http_request(
        const httplib::Request& req, 
        const std::string& session_id
    );
    
    // 解析WebSocket消息
    std::unique_ptr<InternalMessage> parse_websocket_message(
        const std::string& raw_message,
        const std::string& session_id
    );
    
private:
    std::unique_ptr<HttpMessageParser> http_parser_;
    std::unique_ptr<WebSocketMessageParser> websocket_parser_;
    std::unique_ptr<MessageFactory> message_factory_;
};
```

#### 2.2.2 HttpMessageParser (HTTP消息解析器)
```cpp
class HttpMessageParser {
public:
    struct HttpParseResult {
        uint32_t cmd_id;
        std::string body_json;
        std::string token;
        std::string device_id;
        std::string platform;
        bool is_valid;
        std::string error_message;
    };
    
    HttpParseResult parse(const httplib::Request& req);
    
private:
    // 从URL路径提取命令ID
    uint32_t extract_cmd_from_path(const std::string& path);
    
    // 验证HTTP请求格式
    bool validate_http_request(const httplib::Request& req);
    
    // 提取认证信息
    void extract_auth_info(const httplib::Request& req, HttpParseResult& result);
};
```

#### 2.2.3 WebSocketMessageParser (WebSocket消息解析器)
```cpp
class WebSocketMessageParser {
public:
    struct WSParseResult {
        im::base::IMHeader header;
        std::unique_ptr<google::protobuf::Message> message;
        bool is_valid;
        std::string error_message;
    };
    
    WSParseResult parse(const std::string& raw_message);
    
private:
    // 使用现有的ProtobufCodec进行解码
    im::network::ProtobufCodec codec_;
    
    // 验证消息完整性
    bool validate_message(const im::base::IMHeader& header, 
                         const google::protobuf::Message& message);
    
    // 创建对应的消息对象
    std::unique_ptr<google::protobuf::Message> create_message_by_cmd(uint32_t cmd_id);
};
```

#### 2.2.4 InternalMessage (统一内部消息格式)
```cpp
class InternalMessage {
public:
    enum class Protocol {
        HTTP,
        WEBSOCKET
    };
    
    struct MessageContext {
        Protocol protocol;
        std::string session_id;
        std::string client_ip;
        std::chrono::system_clock::time_point receive_time;
    };
    
    // 消息元数据
    uint32_t get_cmd_id() const;
    const std::string& get_from_uid() const;
    const std::string& get_to_uid() const;
    const std::string& get_token() const;
    const std::string& get_device_id() const;
    const std::string& get_platform() const;
    
    // 消息体
    const google::protobuf::Message* get_message() const;
    const std::string& get_json_body() const;
    
    // 上下文信息
    const MessageContext& get_context() const;
    
private:
    // 统一的消息头信息
    im::base::IMHeader header_;
    
    // 消息体（两种格式二选一）
    std::unique_ptr<google::protobuf::Message> protobuf_message_;
    std::string json_body_;
    
    // 上下文信息
    MessageContext context_;
};
```

#### 2.2.5 MessageFactory (消息工厂)
```cpp
class MessageFactory {
public:
    // 从HTTP解析结果创建InternalMessage
    std::unique_ptr<InternalMessage> create_from_http(
        const HttpMessageParser::HttpParseResult& parse_result,
        const std::string& session_id,
        const std::string& client_ip
    );
    
    // 从WebSocket解析结果创建InternalMessage
    std::unique_ptr<InternalMessage> create_from_websocket(
        const WebSocketMessageParser::WSParseResult& parse_result,
        const std::string& session_id,
        const std::string& client_ip
    );
    
private:
    // 根据命令ID创建对应的Protobuf消息对象
    std::unique_ptr<google::protobuf::Message> create_protobuf_message(uint32_t cmd_id);
    
    // 将JSON转换为Protobuf消息
    bool json_to_protobuf(const std::string& json_str, 
                         google::protobuf::Message* message);
};
```

## 3. 消息流程设计

### 3.1 HTTP消息处理流程

```
HTTP请求
  ↓
提取路径中的命令ID (/api/v1/message/send → CMD_SEND_MESSAGE)
  ↓
验证请求格式 (Content-Type, Method等)
  ↓
提取认证信息 (Authorization Header, Query Parameters)
  ↓
解析JSON请求体
  ↓
创建InternalMessage对象
  ↓
传递给RouteManager
```

### 3.2 WebSocket消息处理流程

```
WebSocket二进制消息
  ↓
使用ProtobufCodec解码
  ↓
提取IMHeader和消息体
  ↓
验证消息完整性和权限
  ↓
创建InternalMessage对象
  ↓
传递给RouteManager
```

### 3.3 URL路径与命令映射设计

```
HTTP API路径设计：
/api/v1/auth/login          → CMD_LOGIN
/api/v1/auth/logout         → CMD_LOGOUT
/api/v1/auth/register       → CMD_REGISTER
/api/v1/user/info           → CMD_GET_USER_INFO
/api/v1/user/update         → CMD_UPDATE_USER_INFO
/api/v1/message/send        → CMD_SEND_MESSAGE
/api/v1/message/pull        → CMD_PULL_MESSAGE
/api/v1/message/history     → CMD_MESSAGE_HISTORY
/api/v1/friend/list         → CMD_GET_FRIEND_LIST
/api/v1/friend/add          → CMD_ADD_FRIEND
/api/v1/group/create        → CMD_CREATE_GROUP
/api/v1/group/info          → CMD_GET_GROUP_INFO
```

## 4. 错误处理设计

### 4.1 错误分类

1. **协议错误**：消息格式不正确、解码失败
2. **权限错误**：Token无效、权限不足
3. **参数错误**：必需参数缺失、参数类型错误
4. **系统错误**：内部处理异常

### 4.2 错误响应格式

```json
{
    "error_code": 1001,
    "error_message": "Invalid token",
    "timestamp": 1693123456789,
    "request_id": "req_123456"
}
```

### 4.3 错误处理策略

- HTTP请求：返回标准HTTP状态码和JSON错误信息
- WebSocket消息：发送错误响应消息（使用BaseResponse格式）

## 5. 性能优化考虑

### 5.1 内存管理
- 使用对象池复用InternalMessage对象
- 合理使用移动语义减少拷贝
- 智能指针管理消息生命周期

### 5.2 解析优化
- 缓存路径到命令ID的映射
- 预编译正则表达式
- 使用高效的JSON解析库

### 5.3 并发处理
- 解析器设计为无状态，支持多线程并发
- 避免共享状态，减少锁竞争

## 6. 实现步骤指导

### 第一阶段：基础框架搭建
1. 创建InternalMessage类和相关数据结构
2. 实现MessageFactory基础功能
3. 创建MessageParser主类框架

### 第二阶段：HTTP解析器实现
1. 实现HttpMessageParser类
2. 设计URL路径到命令ID的映射表
3. 实现HTTP请求验证逻辑
4. 添加HTTP错误处理机制

### 第三阶段：WebSocket解析器实现
1. 实现WebSocketMessageParser类
2. 集成现有的ProtobufCodec
3. 实现消息验证逻辑
4. 添加WebSocket错误处理机制

### 第四阶段：集成和测试
1. 将MessageParser集成到GatewayServer中
2. 修改现有的消息处理流程
3. 编写单元测试和集成测试
4. 性能测试和优化

### 第五阶段：扩展和完善
1. 添加消息监控和统计功能
2. 实现消息缓存和批处理
3. 添加限流和防护机制
4. 完善错误处理和日志记录

## 7. 文件结构建议

```
gateway/message_processor/
├── message_parser.hpp/cpp              # 主解析器
├── http_message_parser.hpp/cpp         # HTTP消息解析器
├── websocket_message_parser.hpp/cpp    # WebSocket消息解析器
├── internal_message.hpp/cpp            # 统一内部消息格式
├── message_factory.hpp/cpp             # 消息工厂
├── url_router.hpp/cpp                  # URL路径路由映射
├── error_handler.hpp/cpp               # 错误处理器
└── test/                               # 测试用例
    ├── test_message_parser.cpp
    ├── test_http_parser.cpp
    ├── test_websocket_parser.cpp
    └── test_message_factory.cpp
```

## 8. 与现有组件的集成

### 8.1 与GatewayServer集成
修改GatewayServer的消息处理方法：

```cpp
void GatewayServer::handle_http_request(const httplib::Request& req, httplib::Response& res) {
    // 使用MessageParser解析HTTP请求
    auto internal_msg = message_parser_->parse_http_request(req, session_id);
    
    if (!internal_msg) {
        // 处理解析错误
        res.status = 400;
        res.set_content(error_response, "application/json");
        return;
    }
    
    // 传递给RouteManager处理
    route_mgr_->route_message(std::move(internal_msg));
}

void GatewayServer::handle_websocket_message(std::shared_ptr<WebSocketSession> session,
                                           const std::string& message) {
    // 使用MessageParser解析WebSocket消息
    auto internal_msg = message_parser_->parse_websocket_message(message, session->get_session_id());
    
    if (!internal_msg) {
        // 发送错误响应
        session->send(error_response);
        return;
    }
    
    // 传递给RouteManager处理
    route_mgr_->route_message(std::move(internal_msg));
}
```

### 8.2 与RouteManager集成
RouteManager需要适配新的InternalMessage格式，统一处理不同协议来源的消息。

这个设计充分利用了你现有的优秀架构，同时为Gateway增加了强大的消息解析能力。你觉得这个设计方案如何？接下来我们可以开始逐步实现这些组件。