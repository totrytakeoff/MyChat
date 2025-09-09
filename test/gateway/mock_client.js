const WebSocket = require('ws');
const http = require('http');

class MockWebSocketClient {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.connected = false;
        this.messageCallbacks = [];
        this.closeCallbacks = [];
        this.errorCallbacks = [];
    }

    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);

            this.ws.on('open', () => {
                this.connected = true;
                console.log('WebSocket连接已建立');
                resolve();
            });

            this.ws.on('message', (data) => {
                console.log('收到消息:', data.toString());
                this.messageCallbacks.forEach(callback => callback(data));
            });

            this.ws.on('close', () => {
                this.connected = false;
                console.log('WebSocket连接已关闭');
                this.closeCallbacks.forEach(callback => callback());
            });

            this.ws.on('error', (error) => {
                console.error('WebSocket错误:', error);
                this.errorCallbacks.forEach(callback => callback(error));
                reject(error);
            });
        });
    }

    send(data) {
        if (this.connected && this.ws) {
            this.ws.send(data);
            console.log('发送消息:', data);
        } else {
            console.error('WebSocket未连接');
        }
    }

    close() {
        if (this.ws) {
            this.ws.close();
        }
    }

    onMessage(callback) {
        this.messageCallbacks.push(callback);
    }

    onClose(callback) {
        this.closeCallbacks.push(callback);
    }

    onError(callback) {
        this.errorCallbacks.push(callback);
    }
}

// 模拟HTTP客户端
class MockHttpClient {
    constructor(baseUrl) {
        this.baseUrl = baseUrl;
    }

    async post(path, data) {
        return new Promise((resolve, reject) => {
            const postData = JSON.stringify(data);
            const options = {
                hostname: 'localhost',
                port: 8091, // HTTP端口
                path: path,
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Content-Length': Buffer.byteLength(postData)
                }
            };

            const req = http.request(options, (res) => {
                let responseData = '';
                res.on('data', (chunk) => {
                    responseData += chunk;
                });
                res.on('end', () => {
                    console.log(`收到HTTP响应: ${res.statusCode} ${responseData}`);
                    resolve({ statusCode: res.statusCode, data: responseData });
                });
            });

            req.on('error', (error) => {
                console.error('HTTP请求错误:', error);
                reject(error);
            });

            req.write(postData);
            req.end();
        });
    }

    async get(path) {
        return new Promise((resolve, reject) => {
            const options = {
                hostname: 'localhost',
                port: 8091, // HTTP端口
                path: path,
                method: 'GET'
            };

            const req = http.request(options, (res) => {
                let responseData = '';
                res.on('data', (chunk) => {
                    responseData += chunk;
                });
                res.on('end', () => {
                    console.log(`收到HTTP响应: ${res.statusCode} ${responseData}`);
                    resolve({ statusCode: res.statusCode, data: responseData });
                });
            });

            req.on('error', (error) => {
                console.error('HTTP请求错误:', error);
                reject(error);
            });

            req.end();
        });
    }
}

// 测试函数
async function runTests() {
    console.log('开始网关服务器测试...');

    // 创建WebSocket客户端
    const wsClient = new MockWebSocketClient('ws://localhost:8090');
    
    // 创建HTTP客户端
    const httpClient = new MockHttpClient('http://localhost:8091');

    try {
        // 测试HTTP服务
        console.log('\n=== 测试HTTP服务 ===');
        
        // 测试GET请求
        console.log('测试GET /api/v1/auth/login');
        const getResponse = await httpClient.get('/api/v1/auth/login');
        console.log('GET响应:', getResponse);

        // 测试POST请求
        console.log('测试POST /api/v1/auth/login');
        const postData = {
            username: 'testuser',
            password: 'testpass'
        };
        const postResponse = await httpClient.post('/api/v1/auth/login', postData);
        console.log('POST响应:', postResponse);

        // 测试WebSocket连接
        console.log('\n=== 测试WebSocket连接 ===');
        await wsClient.connect();

        // 发送测试消息
        console.log('发送测试消息');
        wsClient.send('Hello Gateway Server');

        // 等待一段时间接收响应
        await new Promise(resolve => setTimeout(resolve, 1000));

        // 关闭WebSocket连接
        wsClient.close();

        console.log('\n所有测试完成');
    } catch (error) {
        console.error('测试过程中出现错误:', error);
    }
}

// 运行测试
if (require.main === module) {
    runTests().catch(console.error);
}

module.exports = { MockWebSocketClient, MockHttpClient };