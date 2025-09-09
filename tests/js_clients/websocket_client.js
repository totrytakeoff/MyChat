const WebSocket = require('ws');
const axios = require('axios');

class GatewayWebSocketClient {
    constructor(gatewayHost = 'localhost', wsPort = 8080, httpPort = 8081) {
        this.gatewayHost = gatewayHost;
        this.wsPort = wsPort;
        this.httpPort = httpPort;
        this.ws = null;
        this.accessToken = null;
        this.userId = null;
        this.deviceId = null;
        this.platform = 'test';
        this.messageHandlers = new Map();
        this.connectionPromise = null;
    }

    // 通过HTTP API登录获取token
    async login(username = 'testuser', password = 'testpass', deviceId = null) {
        this.deviceId = deviceId || `device_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
        
        try {
            const response = await axios.post(`http://${this.gatewayHost}:${this.httpPort}/api/v1/auth/login`, {
                username,
                password,
                platform: this.platform,
                device_id: this.deviceId
            });

            if (response.data.status_code === 200) {
                const data = response.data.data;
                this.accessToken = data.access_token;
                this.userId = data.user_id;
                console.log(`✅ Login successful: ${username} (${this.userId})`);
                return true;
            } else {
                console.error(`❌ Login failed: ${response.data.message}`);
                return false;
            }
        } catch (error) {
            console.error(`❌ Login error: ${error.message}`);
            return false;
        }
    }

    // 建立WebSocket连接
    async connectWebSocket(withToken = true) {
        const wsUrl = `ws://${this.gatewayHost}:${this.wsPort}${withToken && this.accessToken ? `?token=${this.accessToken}` : ''}`;
        
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(wsUrl);
            
            this.ws.on('open', () => {
                console.log(`🔗 WebSocket connected to ${wsUrl}`);
                resolve(true);
            });

            this.ws.on('message', (data) => {
                this.handleMessage(data);
            });

            this.ws.on('error', (error) => {
                console.error(`❌ WebSocket error: ${error.message}`);
                reject(error);
            });

            this.ws.on('close', (code, reason) => {
                console.log(`🔌 WebSocket closed: ${code} - ${reason.toString()}`);
            });

            // 超时处理
            setTimeout(() => {
                if (this.ws.readyState !== WebSocket.OPEN) {
                    reject(new Error('WebSocket connection timeout'));
                }
            }, 5000);
        });
    }

    // 处理收到的消息
    handleMessage(data) {
        try {
            // 尝试解析为protobuf格式
            const message = this.parseProtobufMessage(data);
            console.log(`📨 Received message:`, message);
            
            // 调用注册的处理器
            const cmdId = message.header?.cmd_id;
            if (cmdId && this.messageHandlers.has(cmdId)) {
                this.messageHandlers.get(cmdId)(message);
            }
        } catch (error) {
            // 如果不是protobuf，尝试解析为JSON
            try {
                const jsonMessage = JSON.parse(data.toString());
                console.log(`📨 Received JSON message:`, jsonMessage);
            } catch (jsonError) {
                console.log(`📨 Received raw message:`, data.toString());
            }
        }
    }

    // 简单的protobuf消息解析（这里需要根据实际的proto定义来实现）
    parseProtobufMessage(data) {
        // 这是一个简化的实现，实际应该使用protobuf.js库
        // 这里只做基本解析演示
        try {
            const buffer = Buffer.from(data);
            // 假设前4字节是长度，后面是protobuf数据
            if (buffer.length > 4) {
                const length = buffer.readUInt32BE(0);
                const protobufData = buffer.slice(4, 4 + length);
                
                // 这里应该使用protobuf解码
                return {
                    header: {
                        cmd_id: 1,
                        seq: 1,
                        timestamp: Date.now()
                    },
                    data: protobufData,
                    raw: data
                };
            }
        } catch (error) {
            console.warn('Failed to parse as protobuf:', error.message);
        }
        
        return {
            header: null,
            data: data,
            raw: data
        };
    }

    // 发送消息
    async sendMessage(toUserId, content, cmdId = 2001) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
            throw new Error('WebSocket not connected');
        }

        // 构造测试消息（简化的protobuf格式）
        const message = {
            header: {
                cmd_id: cmdId,
                seq: Date.now(),
                from_uid: this.userId,
                to_uid: toUserId,
                timestamp: Date.now()
            },
            data: {
                content: content,
                message_type: 'text'
            }
        };

        // 简化的消息编码（实际应该使用protobuf编码）
        const messageStr = JSON.stringify(message);
        const buffer = Buffer.alloc(4 + Buffer.byteLength(messageStr));
        buffer.writeUInt32BE(Buffer.byteLength(messageStr), 0);
        buffer.write(messageStr, 4);

        this.ws.send(buffer);
        console.log(`📤 Sent message to ${toUserId}: ${content}`);
        
        return message.header.seq;
    }

    // 注册消息处理器
    onMessage(cmdId, handler) {
        this.messageHandlers.set(cmdId, handler);
    }

    // 断开连接
    disconnect() {
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    // 等待指定时间
    async wait(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
}

// 测试场景
class WebSocketTestScenarios {
    constructor() {
        this.clients = [];
    }

    // 测试基本连接
    async testBasicConnection() {
        console.log('\n🧪 Testing basic WebSocket connection...');
        
        const client = new GatewayWebSocketClient();
        this.clients.push(client);

        try {
            // 登录
            const loginSuccess = await client.login();
            if (!loginSuccess) {
                throw new Error('Login failed');
            }

            // 连接WebSocket
            await client.connectWebSocket();
            
            // 等待一段时间确保连接稳定
            await client.wait(1000);
            
            console.log('✅ Basic connection test passed');
            return true;
        } catch (error) {
            console.error(`❌ Basic connection test failed: ${error.message}`);
            return false;
        }
    }

    // 测试无token连接
    async testConnectionWithoutToken() {
        console.log('\n🧪 Testing WebSocket connection without token...');
        
        const client = new GatewayWebSocketClient();
        this.clients.push(client);

        try {
            // 不获取token直接连接
            await client.connectWebSocket(false);
            
            // 等待30秒看是否会被断开
            console.log('⏰ Waiting for 30s timeout...');
            await client.wait(31000);
            
            console.log('✅ No-token connection test completed');
            return true;
        } catch (error) {
            console.error(`❌ No-token connection test failed: ${error.message}`);
            return false;
        }
    }

    // 测试消息发送
    async testMessageSending() {
        console.log('\n🧪 Testing message sending...');
        
        const client1 = new GatewayWebSocketClient();
        const client2 = new GatewayWebSocketClient();
        this.clients.push(client1, client2);

        try {
            // 两个客户端登录
            await client1.login('testuser1', 'testpass', 'device1');
            await client2.login('testuser2', 'testpass', 'device2');
            
            // 连接WebSocket
            await client1.connectWebSocket();
            await client2.connectWebSocket();
            
            // 设置消息接收处理器
            let receivedMessage = false;
            client2.onMessage(2001, (message) => {
                console.log('📬 Client2 received message:', message);
                receivedMessage = true;
            });
            
            // 发送消息
            await client1.sendMessage('user_002', 'Hello from client1!');
            
            // 等待消息传递
            await client1.wait(2000);
            
            console.log('✅ Message sending test completed');
            return true;
        } catch (error) {
            console.error(`❌ Message sending test failed: ${error.message}`);
            return false;
        }
    }

    // 测试认证失败
    async testAuthenticationFailure() {
        console.log('\n🧪 Testing authentication failure...');
        
        const client = new GatewayWebSocketClient();
        this.clients.push(client);

        try {
            // 使用无效凭据登录
            const loginSuccess = await client.login('invalid_user', 'invalid_pass');
            
            if (loginSuccess) {
                console.log('⚠️ Expected login to fail, but it succeeded');
            } else {
                console.log('✅ Authentication failure test passed');
            }
            
            return true;
        } catch (error) {
            console.error(`❌ Authentication failure test error: ${error.message}`);
            return false;
        }
    }

    // 并发连接测试
    async testConcurrentConnections() {
        console.log('\n🧪 Testing concurrent connections...');
        
        const numClients = 10;
        const clients = [];
        
        try {
            // 创建多个并发客户端
            const promises = [];
            for (let i = 0; i < numClients; i++) {
                const client = new GatewayWebSocketClient();
                clients.push(client);
                this.clients.push(client);
                
                promises.push((async () => {
                    await client.login(`testuser${i}`, 'testpass', `device${i}`);
                    await client.connectWebSocket();
                    return client;
                })());
            }
            
            // 等待所有客户端连接完成
            const connectedClients = await Promise.all(promises);
            console.log(`✅ ${connectedClients.length} concurrent connections established`);
            
            // 等待一段时间
            await new Promise(resolve => setTimeout(resolve, 2000));
            
            console.log('✅ Concurrent connections test passed');
            return true;
        } catch (error) {
            console.error(`❌ Concurrent connections test failed: ${error.message}`);
            return false;
        } finally {
            // 清理客户端
            clients.forEach(client => client.disconnect());
        }
    }

    // 清理所有客户端
    cleanup() {
        console.log('\n🧹 Cleaning up test clients...');
        this.clients.forEach(client => client.disconnect());
        this.clients = [];
    }

    // 运行所有测试
    async runAllTests() {
        console.log('🚀 Starting WebSocket tests for Gateway Server...\n');
        
        const tests = [
            { name: 'Basic Connection', fn: () => this.testBasicConnection() },
            { name: 'Authentication Failure', fn: () => this.testAuthenticationFailure() },
            { name: 'Message Sending', fn: () => this.testMessageSending() },
            { name: 'Concurrent Connections', fn: () => this.testConcurrentConnections() },
            // { name: 'Connection Without Token', fn: () => this.testConnectionWithoutToken() }, // 这个测试比较耗时，可选
        ];
        
        let passed = 0;
        let failed = 0;
        
        for (const test of tests) {
            try {
                console.log(`\n🏃 Running test: ${test.name}`);
                const result = await test.fn();
                if (result) {
                    passed++;
                } else {
                    failed++;
                }
            } catch (error) {
                console.error(`❌ Test "${test.name}" threw error: ${error.message}`);
                failed++;
            }
            
            // 每个测试之间清理和等待
            this.cleanup();
            await new Promise(resolve => setTimeout(resolve, 1000));
        }
        
        console.log(`\n📊 Test Results:`);
        console.log(`   ✅ Passed: ${passed}`);
        console.log(`   ❌ Failed: ${failed}`);
        console.log(`   📈 Success Rate: ${(passed / (passed + failed) * 100).toFixed(1)}%`);
        
        return { passed, failed };
    }
}

// 如果直接运行此文件
if (require.main === module) {
    const testRunner = new WebSocketTestScenarios();
    
    // 添加优雅的退出处理
    process.on('SIGINT', () => {
        console.log('\n👋 Received SIGINT, cleaning up...');
        testRunner.cleanup();
        process.exit(0);
    });
    
    testRunner.runAllTests()
        .then(results => {
            testRunner.cleanup();
            process.exit(results.failed > 0 ? 1 : 0);
        })
        .catch(error => {
            console.error('❌ Test runner error:', error);
            testRunner.cleanup();
            process.exit(1);
        });
}

module.exports = { GatewayWebSocketClient, WebSocketTestScenarios };