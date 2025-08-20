# Gateway消息解析器集成指导

## 概述

本文档指导如何将新实现的消息解析器集成到现有的GatewayServer中，实现统一的消息处理流程。

## 集成步骤

### 第一步：更新GatewayServer类

#### 1.1 修改头文件 (gateway_server.hpp)

```cpp
#include "../message_processor/message_processor.hpp"

class GatewayServer {
public:
    // ... 现有方法 ...

private:
    // ... 现有成员 ...
    
    // 添加消息解析器
    std::unique_ptr<MessageProcessor> message_processor_;
    
    // 更新消息处理方法签名
    void handle_websocket_message(std::shared_ptr<WebSocketSession> session,
                                  const std::string& message);
    void handle_tcp_message(std::shared_ptr<TCPSession> session, 
                           const std::string& message);
    void handle_http_request(const httplib::Request& req, 
                           httplib::Response& res);
                           
    // 添加统一的消息路由方法
    void route_internal_message(std::unique_ptr<InternalMessage> internal_msg,
                               std::shared_ptr<WebSocketSession> ws_session = nullptr,
                               httplib::Response* http_response = nullptr);
};
```

#### 1.2 修改实现文件 (gateway_server.cpp)

```cpp
// 在构造函数中初始化消息解析器
GatewayServer::GatewayServer() {
    // ... 现有初始化代码 ...
    
    message_processor_ = std::make_unique<MessageProcessor>();
}

// 更新HTTP请求处理
void GatewayServer::handle_http_request(const httplib::Request& req, httplib::Response& res) {
    try {
        // 生成会话ID（可以使用现有的会话管理逻辑）
        std::string session_id = generate_http_session_id(req);
        
        // 使用消息解析器解析HTTP请求
        auto internal_msg = message_processor_->parse_http_request(req, session_id);
        
        if (!internal_msg) {
            // 解析失败，返回错误响应
            res.status = 400;
            res.set_content(R"({"error_code": 1, "error_message": "Invalid request format"})", 
                          "application/json");
            return;
        }
        
        // 路由到内部处理
        route_internal_message(std::move(internal_msg), nullptr, &res);
        
    } catch (const std::exception& e) {
        // 异常处理
        res.status = 500;
        res.set_content(R"({"error_code": 500, "error_message": "Internal server error"})", 
                      "application/json");
    }
}

// 更新WebSocket消息处理
void GatewayServer::handle_websocket_message(std::shared_ptr<WebSocketSession> session,
                                           const std::string& message) {
    try {
        // 使用消息解析器解析WebSocket消息
        auto internal_msg = message_processor_->parse_websocket_message(message, 
                                                                       session->get_session_id());
        
        if (!internal_msg) {
            // 解析失败，发送错误响应
            send_websocket_error(session, "Invalid message format");
            return;
        }
        
        // 路由到内部处理
        route_internal_message(std::move(internal_msg), session, nullptr);
        
    } catch (const std::exception& e) {
        // 异常处理
        send_websocket_error(session, "Internal server error");
    }
}

// 统一的消息路由方法
void GatewayServer::route_internal_message(std::unique_ptr<InternalMessage> internal_msg,
                                         std::shared_ptr<WebSocketSession> ws_session,
                                         httplib::Response* http_response) {
    
    // 根据命令ID路由到不同的处理器
    uint32_t cmd_id = internal_msg->get_cmd_id();
    
    // 这里需要根据你的RouteManager设计来调用
    // 示例：
    route_mgr_->route_message(std::move(internal_msg), ws_session, http_response);
}
```

### 第二步：更新RouteManager

#### 2.1 修改RouteManager接口

```cpp
class RouteManager {
public:
    // 更新路由方法，支持InternalMessage
    void route_message(std::unique_ptr<InternalMessage> internal_msg,
                      std::shared_ptr<WebSocketSession> ws_session = nullptr,
                      httplib::Response* http_response = nullptr);
                      
private:
    // 按功能模块分发消息
    void route_to_auth_service(const InternalMessage& msg, ResponseContext& ctx);
    void route_to_message_service(const InternalMessage& msg, ResponseContext& ctx);
    void route_to_friend_service(const InternalMessage& msg, ResponseContext& ctx);
    void route_to_group_service(const InternalMessage& msg, ResponseContext& ctx);
    
    // 响应上下文，用于统一处理不同协议的响应
    struct ResponseContext {
        InternalMessage::Protocol protocol;
        std::shared_ptr<WebSocketSession> ws_session;
        httplib::Response* http_response;
        std::string session_id;
    };
};
```

#### 2.2 实现统一的响应处理

```cpp
void RouteManager::route_message(std::unique_ptr<InternalMessage> internal_msg,
                               std::shared_ptr<WebSocketSession> ws_session,
                               httplib::Response* http_response) {
    
    // 创建响应上下文
    ResponseContext ctx;
    ctx.protocol = internal_msg->get_context().protocol;
    ctx.ws_session = ws_session;
    ctx.http_response = http_response;
    ctx.session_id = internal_msg->get_context().session_id;
    
    // 根据命令ID分发到不同的服务
    uint32_t cmd_id = internal_msg->get_cmd_id();
    
    using namespace im::command;
    if (cmd_id >= CMD_LOGIN && cmd_id <= CMD_USER_OFFLINE) {
        route_to_auth_service(*internal_msg, ctx);
    } else if (cmd_id >= CMD_SEND_MESSAGE && cmd_id <= CMD_MESSAGE_HISTORY) {
        route_to_message_service(*internal_msg, ctx);
    } else if (cmd_id >= CMD_ADD_FRIEND && cmd_id <= CMD_SEARCH_USER) {
        route_to_friend_service(*internal_msg, ctx);
    } else if (cmd_id >= CMD_CREATE_GROUP && cmd_id <= CMD_SET_GROUP_ADMIN) {
        route_to_group_service(*internal_msg, ctx);
    } else {
        send_error_response(ctx, "Unsupported command");
    }
}

// 统一的错误响应方法
void RouteManager::send_error_response(const ResponseContext& ctx, const std::string& error_msg) {
    if (ctx.protocol == InternalMessage::Protocol::HTTP && ctx.http_response) {
        // HTTP错误响应
        ctx.http_response->status = 400;
        nlohmann::json error_json = {
            {"error_code", 1},
            {"error_message", error_msg},
            {"timestamp", std::time(nullptr)}
        };
        ctx.http_response->set_content(error_json.dump(), "application/json");
        
    } else if (ctx.protocol == InternalMessage::Protocol::WEBSOCKET && ctx.ws_session) {
        // WebSocket错误响应
        im::base::BaseResponse error_response;
        error_response.set_error_code(im::base::PARAM_ERROR);
        error_response.set_error_message(error_msg);
        
        // 使用ProtobufCodec编码并发送
        im::base::IMHeader header;
        header.set_cmd_id(im::command::CMD_SERVER_NOTIFY);
        // ... 设置其他头部信息 ...
        
        std::string encoded_response;
        if (codec_.encode(header, error_response, encoded_response)) {
            ctx.ws_session->send(encoded_response);
        }
    }
}
```

### 第三步：集成认证和权限检查

#### 3.1 在消息路由前添加认证检查

```cpp
bool RouteManager::authenticate_request(const InternalMessage& msg) {
    const std::string& token = msg.get_token();
    
    // 跳过不需要认证的命令
    uint32_t cmd_id = msg.get_cmd_id();
    if (cmd_id == im::command::CMD_LOGIN || 
        cmd_id == im::command::CMD_REGISTER ||
        cmd_id == im::command::CMD_HEARTBEAT) {
        return true;
    }
    
    // 验证token
    if (token.empty()) {
        return false;
    }
    
    // 调用AuthManager验证token
    return auth_mgr_->validate_token(token, msg.get_device_id());
}
```

### 第四步：更新CMakeLists.txt

确保gateway模块能找到并链接消息处理器：

```cmake
# 在gateway的CMakeLists.txt中添加
target_include_directories(gateway_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/message_processor
)

# 添加源文件
target_sources(gateway_server PRIVATE
    message_processor/message_processor.cpp
)
```

### 第五步：配置和测试

#### 5.1 添加配置选项

在config.json中添加消息处理相关配置：

```json
{
    "message_processor": {
        "enable_json_validation": true,
        "max_message_size": 10485760,
        "timeout_seconds": 30,
        "enable_debug_logging": false
    }
}
```

#### 5.2 测试步骤

1. **编译测试**：
```bash
cd gateway/message_processor/test
mkdir build && cd build
cmake ..
make
./test_message_parser
```

2. **集成测试**：
```bash
# 启动Gateway服务
./gateway_server

# 测试HTTP接口
curl -X POST http://localhost:8080/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer test_token" \
  -d '{"username": "test", "password": "123456"}'

# 测试WebSocket连接（需要WebSocket客户端）
```

## 注意事项

### 1. 错误处理
- 确保所有异常都被正确捕获和处理
- 为不同的错误类型返回适当的HTTP状态码
- WebSocket错误要发送structured error messages

### 2. 性能优化
- 消息解析器是无状态的，支持多线程并发
- 考虑添加消息缓存机制
- 监控消息解析的性能指标

### 3. 安全性
- 验证所有输入参数
- 实施请求频率限制
- 记录安全相关的事件

### 4. 日志记录
- 记录所有消息解析错误
- 添加性能监控日志
- 区分不同级别的日志信息

### 5. 扩展性
- 新增API时，只需在URL_CMD_MAPPING中添加映射
- 新增protobuf消息类型时，更新create_message_by_cmd方法
- 保持向后兼容性

## 下一步计划

1. **完成基础集成**：按照本指导完成GatewayServer的集成
2. **添加监控**：集成prometheus指标收集
3. **性能测试**：进行压力测试和性能调优
4. **安全加固**：添加更完善的安全机制
5. **文档完善**：编写API文档和运维手册

完成这些步骤后，你的Gateway将具备完整的消息解析和路由能力，能够统一处理HTTP和WebSocket两种协议的消息。