# MyChat Gateway Demo

这是一个完整的网关架构演示项目，基于你现有的工具类和网络组件实现了一套可运行的网关服务。

## 🏗️ 架构特性

### ✅ 已实现的功能

- **双协议支持**: HTTP REST API + WebSocket 实时通信
- **Token认证**: 基于时间戳和密钥的Token生成和验证
- **请求路由**: 根据命令ID自动路由到对应微服务
- **连接管理**: 统一管理WebSocket和HTTP连接
- **消息处理**: 统一的消息解析、验证和封装
- **健康监控**: 服务健康检查和统计信息收集
- **异步处理**: 基于线程池的高并发处理

### 🧩 核心模块

1. **MessageProcessor** - 消息格式转换器
2. **ConnectionManager** - 连接和会话管理
3. **AuthManager** - 用户认证和Token管理
4. **RouteManager** - 请求路由和服务调用
5. **GatewayServer** - 主服务整合器

## 🚀 快速开始

### 环境要求

- **系统**: Linux (Ubuntu 18.04+ 推荐)
- **编译器**: GCC 7+ 或 Clang 6+
- **CMake**: 3.16+
- **依赖库**:
  - libprotobuf-dev
  - protobuf-compiler
  - libboost-all-dev
  - libssl-dev (可选，用于加密)

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libprotobuf-dev \
    protobuf-compiler \
    libboost-all-dev \
    libssl-dev \
    pkg-config \
    wget
```

### 构建和运行

```bash
# 1. 进入demo目录
cd gateway/demo

# 2. 运行构建脚本（自动下载依赖）
chmod +x build.sh
./build.sh

# 3. 启动网关服务
./run_demo.sh
```

或者手动构建：

```bash
# 手动构建步骤
mkdir -p third_party/httplib
wget -O third_party/httplib/httplib.h \
    https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行
cd bin
./gateway_server config/gateway_config.json
```

## 📡 API 测试

### HTTP REST API

```bash
# 健康检查
curl http://localhost:8080/health

# 用户登录 (demo账号: test/test)
curl -X POST http://localhost:8080/api/v1/login \
     -H 'Content-Type: application/json' \
     -d '{"username":"test","password":"test","device_id":"demo_device"}'

# 获取用户信息 (需要从登录响应中获取token)
curl -H "Authorization: Bearer YOUR_TOKEN" \
     http://localhost:8080/api/v1/user/info

# 发送消息 (需要token)
curl -X POST http://localhost:8080/api/v1/message/send \
     -H "Authorization: Bearer YOUR_TOKEN" \
     -H 'Content-Type: application/json' \
     -d '{"to_user":"user_2","content":"Hello from HTTP API!"}'

# 查看服务器统计
curl http://localhost:8080/stats
```

### WebSocket API

```javascript
// JavaScript WebSocket客户端示例
const ws = new WebSocket('ws://localhost:8081');

// 连接建立后发送登录消息
ws.onopen = function() {
    const loginMsg = {
        header: {
            version: "1.0",
            seq: 1,
            cmd_id: 1001, // CMD_LOGIN
            timestamp: Date.now()
        },
        payload: {
            username: "test",
            password: "test",
            device_id: "web_client"
        }
    };
    ws.send(JSON.stringify(loginMsg));
};

// 处理响应消息
ws.onmessage = function(event) {
    const response = JSON.parse(event.data);
    console.log('Received:', response);
    
    // 如果登录成功，可以发送其他请求
    if (response.header.cmd_id === 1001 && response.response.error_code === 0) {
        const token = JSON.parse(response.response.data).token;
        console.log('Login successful, token:', token);
    }
};
```

## 📊 监控和调试

### 查看日志

```bash
# 实时查看日志
tail -f build/bin/logs/gateway_server.log

# 查看连接管理器日志
tail -f build/bin/logs/connection_manager.log

# 查看认证管理器日志  
tail -f build/bin/logs/auth_manager.log

# 查看路由管理器日志
tail -f build/bin/logs/route_manager.log
```

### 统计信息API

```bash
# 服务器整体统计
curl http://localhost:8080/stats | jq .

# 健康检查
curl http://localhost:8080/health | jq .
```

## 🔧 配置说明

主要配置文件：`config/gateway_config.json`

```json
{
    "gateway": {
        "http_port": 8080,        // HTTP服务端口
        "websocket_port": 8081,   // WebSocket服务端口
        "worker_threads": 4,      // 工作线程数
        "max_connections": 1000   // 最大连接数
    },
    "auth": {
        "token_expire_seconds": 86400,  // Token过期时间
        "max_tokens_per_user": 5,       // 每用户最大Token数
        "auto_refresh": true            // 是否自动刷新Token
    },
    "services": {
        "user_service": {
            "host": "127.0.0.1",
            "port": 9001,
            "timeout_ms": 5000
        }
        // ... 其他微服务配置
    }
}
```

## 📁 项目结构

```
gateway/demo/
├── src/                           # 源代码
│   ├── gateway_server.hpp/cpp     # 主服务器类
│   ├── connection_manager.hpp/cpp # 连接管理
│   ├── auth_manager.hpp/cpp       # 认证管理
│   ├── route_manager.hpp/cpp      # 路由管理
│   ├── message_processor.hpp/cpp  # 消息处理
│   └── main.cpp                   # 程序入口
├── config/                        # 配置文件
│   └── gateway_config.json        # 网关配置
├── third_party/                   # 第三方库
│   └── httplib/                   # HTTP库
├── scripts/                       # 构建脚本
├── CMakeLists.txt                 # 构建配置
├── build.sh                       # 构建脚本
├── run_demo.sh                    # 启动脚本
└── README.md                      # 本文档
```

## 🧪 测试场景

### 1. 基础连接测试

```bash
# 测试HTTP服务
curl -v http://localhost:8080/health

# 测试WebSocket连接（使用wscat工具）
# npm install -g wscat
wscat -c ws://localhost:8081
```

### 2. 认证流程测试

```bash
# 1. 用户登录
TOKEN=$(curl -s -X POST http://localhost:8080/api/v1/login \
    -H 'Content-Type: application/json' \
    -d '{"username":"test","password":"test"}' | \
    jq -r '.data.token')

echo "Token: $TOKEN"

# 2. 使用Token访问受保护资源
curl -H "Authorization: Bearer $TOKEN" \
    http://localhost:8080/api/v1/user/info

# 3. 登出
curl -X POST http://localhost:8080/api/v1/logout \
    -H "Authorization: Bearer $TOKEN"
```

### 3. 并发连接测试

```bash
# 使用Apache Bench进行压力测试
ab -n 1000 -c 10 http://localhost:8080/health

# 或使用curl进行简单并发测试
for i in {1..10}; do
    curl http://localhost:8080/health &
done
wait
```

## 🐛 常见问题

### 编译问题

**Q: 找不到httplib.h**
```bash
# 手动下载
mkdir -p third_party/httplib
wget -O third_party/httplib/httplib.h \
    https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

**Q: protobuf编译错误**
```bash
# 重新生成protobuf文件
cd ../../common/proto
protoc --cpp_out=. *.proto
```

### 运行时问题

**Q: 端口被占用**
```bash
# 检查端口占用
sudo netstat -tlnp | grep :8080
sudo netstat -tlnp | grep :8081

# 修改配置文件中的端口号
```

**Q: 权限错误**
```bash
# 确保日志目录可写
mkdir -p build/bin/logs
chmod 755 build/bin/logs
```

## 🔮 后续扩展方向

### 立即可以添加的功能

1. **数据库集成**
   - 添加MySQL/PostgreSQL连接池
   - 实现用户信息持久化
   - 添加消息历史存储

2. **Redis缓存**
   - Token缓存和分布式会话
   - 消息队列和离线消息
   - 限流和防重放

3. **服务发现**
   - 集成Consul/etcd
   - 动态服务注册和发现
   - 健康检查和故障转移

### 生产级别功能

1. **安全性**
   - HTTPS/WSS支持
   - JWT Token实现
   - API限流和防护

2. **可观测性**
   - Prometheus指标导出
   - 分布式链路追踪
   - 结构化日志

3. **高可用性**
   - 负载均衡
   - 故障转移
   - 配置热更新

## 📝 开发指南

这个demo展示了如何：

1. **模块化设计** - 每个核心功能独立成模块
2. **接口抽象** - 使用抽象接口便于扩展
3. **错误处理** - 完善的异常处理和日志记录
4. **配置管理** - 灵活的配置系统
5. **并发安全** - 线程安全的设计模式

你可以基于这个架构继续开发：

- 实现真正的微服务后端
- 添加更多的中间件功能
- 扩展到分布式架构
- 优化性能和扩展性

## 📄 许可证

本demo项目仅供学习和参考使用。

---

**Happy Coding! 🚀**

如果你有任何问题或建议，欢迎提出issue或讨论！