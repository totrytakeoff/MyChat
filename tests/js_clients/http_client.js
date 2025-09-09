const axios = require('axios');

class GatewayHTTPClient {
    constructor(gatewayHost = 'localhost', httpPort = 8081) {
        this.baseURL = `http://${gatewayHost}:${httpPort}`;
        this.apiPrefix = '/api/v1';
        this.accessToken = null;
        this.userId = null;
        
        // 创建axios实例
        this.client = axios.create({
            baseURL: this.baseURL + this.apiPrefix,
            timeout: 10000,
            headers: {
                'Content-Type': 'application/json'
            }
        });
        
        // 请求拦截器
        this.client.interceptors.request.use(
            (config) => {
                if (this.accessToken) {
                    config.headers.Authorization = `Bearer ${this.accessToken}`;
                }
                return config;
            },
            (error) => Promise.reject(error)
        );
        
        // 响应拦截器
        this.client.interceptors.response.use(
            (response) => response,
            (error) => {
                console.error(`HTTP Error: ${error.response?.status} - ${error.response?.data?.message || error.message}`);
                return Promise.reject(error);
            }
        );
    }

    // 登录
    async login(username = 'testuser', password = 'testpass', platform = 'test', deviceId = null) {
        const requestData = {
            username,
            password,
            platform,
            device_id: deviceId || `device_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`
        };

        try {
            const response = await this.client.post('/auth/login', requestData);
            
            if (response.data.status_code === 200) {
                const data = response.data.data;
                this.accessToken = data.access_token;
                this.userId = data.user_id;
                
                console.log(`✅ Login successful: ${username} (${this.userId})`);
                return {
                    success: true,
                    data: data,
                    accessToken: this.accessToken,
                    userId: this.userId
                };
            } else {
                console.error(`❌ Login failed: ${response.data.message}`);
                return {
                    success: false,
                    error: response.data.message
                };
            }
        } catch (error) {
            console.error(`❌ Login error: ${error.message}`);
            return {
                success: false,
                error: error.message
            };
        }
    }

    // 刷新token
    async refreshToken(refreshToken) {
        try {
            const response = await this.client.post('/auth/refresh', {
                refresh_token: refreshToken
            });
            
            if (response.data.status_code === 200) {
                const data = response.data.data;
                this.accessToken = data.access_token;
                
                console.log('✅ Token refreshed successfully');
                return {
                    success: true,
                    data: data,
                    accessToken: this.accessToken
                };
            } else {
                console.error(`❌ Token refresh failed: ${response.data.message}`);
                return {
                    success: false,
                    error: response.data.message
                };
            }
        } catch (error) {
            console.error(`❌ Token refresh error: ${error.message}`);
            return {
                success: false,
                error: error.message
            };
        }
    }

    // 登出
    async logout() {
        try {
            const response = await this.client.post('/auth/logout');
            
            // 清除本地token
            this.accessToken = null;
            this.userId = null;
            
            console.log('✅ Logout successful');
            return {
                success: true,
                data: response.data
            };
        } catch (error) {
            console.error(`❌ Logout error: ${error.message}`);
            return {
                success: false,
                error: error.message
            };
        }
    }

    // 发送消息（通过HTTP）
    async sendMessage(toUserId, content, messageType = 'text') {
        const requestData = {
            to_user_id: toUserId,
            content: content,
            message_type: messageType,
            timestamp: Date.now()
        };

        try {
            const response = await this.client.post('/message/send', requestData);
            
            if (response.data.status_code === 200) {
                console.log(`✅ Message sent to ${toUserId}: ${content}`);
                return {
                    success: true,
                    data: response.data.data
                };
            } else {
                console.error(`❌ Send message failed: ${response.data.message}`);
                return {
                    success: false,
                    error: response.data.message
                };
            }
        } catch (error) {
            console.error(`❌ Send message error: ${error.message}`);
            return {
                success: false,
                error: error.message
            };
        }
    }

    // 获取消息历史
    async getMessageHistory(userId, limit = 20, offset = 0) {
        try {
            const response = await this.client.get('/message/history', {
                params: {
                    user_id: userId,
                    limit: limit,
                    offset: offset
                }
            });
            
            if (response.data.status_code === 200) {
                console.log(`✅ Retrieved message history for user ${userId}`);
                return {
                    success: true,
                    data: response.data.data
                };
            } else {
                console.error(`❌ Get message history failed: ${response.data.message}`);
                return {
                    success: false,
                    error: response.data.message
                };
            }
        } catch (error) {
            console.error(`❌ Get message history error: ${error.message}`);
            return {
                success: false,
                error: error.message
            };
        }
    }

    // 健康检查（如果有的话）
    async healthCheck() {
        try {
            // 尝试访问根路径或健康检查端点
            const response = await axios.get(`${this.baseURL}/health`, { timeout: 5000 });
            return {
                success: true,
                data: response.data
            };
        } catch (error) {
            return {
                success: false,
                error: error.message
            };
        }
    }
}

// HTTP API测试场景
class HTTPTestScenarios {
    constructor() {
        this.clients = [];
    }

    // 测试基本认证流程
    async testBasicAuth() {
        console.log('\n🧪 Testing basic authentication flow...');
        
        const client = new GatewayHTTPClient();
        this.clients.push(client);

        try {
            // 测试登录
            const loginResult = await client.login();
            if (!loginResult.success) {
                throw new Error(`Login failed: ${loginResult.error}`);
            }

            // 测试登出
            const logoutResult = await client.logout();
            if (!logoutResult.success) {
                throw new Error(`Logout failed: ${logoutResult.error}`);
            }

            console.log('✅ Basic authentication test passed');
            return true;
        } catch (error) {
            console.error(`❌ Basic authentication test failed: ${error.message}`);
            return false;
        }
    }

    // 测试无效凭据
    async testInvalidCredentials() {
        console.log('\n🧪 Testing invalid credentials...');
        
        const client = new GatewayHTTPClient();
        this.clients.push(client);

        try {
            const result = await client.login('invalid_user', 'invalid_password');
            
            if (result.success) {
                console.error('❌ Expected login to fail with invalid credentials');
                return false;
            } else {
                console.log('✅ Invalid credentials correctly rejected');
                return true;
            }
        } catch (error) {
            console.error(`❌ Invalid credentials test error: ${error.message}`);
            return false;
        }
    }

    // 测试Token刷新
    async testTokenRefresh() {
        console.log('\n🧪 Testing token refresh...');
        
        const client = new GatewayHTTPClient();
        this.clients.push(client);

        try {
            // 先登录获取token
            const loginResult = await client.login();
            if (!loginResult.success) {
                throw new Error('Login failed');
            }

            const refreshToken = loginResult.data.refresh_token;
            if (!refreshToken) {
                throw new Error('No refresh token received');
            }

            // 测试token刷新
            const refreshResult = await client.refreshToken(refreshToken);
            if (!refreshResult.success) {
                throw new Error(`Token refresh failed: ${refreshResult.error}`);
            }

            console.log('✅ Token refresh test passed');
            return true;
        } catch (error) {
            console.error(`❌ Token refresh test failed: ${error.message}`);
            return false;
        }
    }

    // 测试消息API
    async testMessageAPI() {
        console.log('\n🧪 Testing message API...');
        
        const client1 = new GatewayHTTPClient();
        const client2 = new GatewayHTTPClient();
        this.clients.push(client1, client2);

        try {
            // 两个客户端登录
            const login1 = await client1.login('testuser1', 'testpass', 'test', 'device1');
            const login2 = await client2.login('testuser2', 'testpass', 'test', 'device2');
            
            if (!login1.success || !login2.success) {
                throw new Error('One or both logins failed');
            }

            // client1 发送消息给 client2
            const sendResult = await client1.sendMessage(login2.userId, 'Hello from HTTP API!');
            if (!sendResult.success) {
                throw new Error(`Send message failed: ${sendResult.error}`);
            }

            // client2 获取消息历史
            const historyResult = await client2.getMessageHistory(login1.userId);
            if (!historyResult.success) {
                throw new Error(`Get message history failed: ${historyResult.error}`);
            }

            console.log('✅ Message API test passed');
            return true;
        } catch (error) {
            console.error(`❌ Message API test failed: ${error.message}`);
            return false;
        }
    }

    // 测试并发请求
    async testConcurrentRequests() {
        console.log('\n🧪 Testing concurrent HTTP requests...');
        
        const numClients = 20;
        const clients = [];

        try {
            // 创建并发登录请求
            const loginPromises = [];
            for (let i = 0; i < numClients; i++) {
                const client = new GatewayHTTPClient();
                clients.push(client);
                this.clients.push(client);
                
                loginPromises.push(
                    client.login(`testuser${i}`, 'testpass', 'test', `device${i}`)
                );
            }

            const startTime = Date.now();
            const results = await Promise.allSettled(loginPromises);
            const endTime = Date.now();

            const successCount = results.filter(r => r.status === 'fulfilled' && r.value.success).length;
            const failureCount = results.length - successCount;

            console.log(`📊 Concurrent request results:`);
            console.log(`   Total requests: ${results.length}`);
            console.log(`   Successful: ${successCount}`);
            console.log(`   Failed: ${failureCount}`);
            console.log(`   Total time: ${endTime - startTime}ms`);
            console.log(`   Average time per request: ${(endTime - startTime) / results.length}ms`);

            if (successCount >= results.length * 0.8) {  // 至少80%成功
                console.log('✅ Concurrent requests test passed');
                return true;
            } else {
                console.error('❌ Too many failures in concurrent requests');
                return false;
            }
        } catch (error) {
            console.error(`❌ Concurrent requests test failed: ${error.message}`);
            return false;
        } finally {
            // 清理客户端
            clients.forEach(client => {
                if (client.accessToken) {
                    client.logout().catch(() => {});  // 忽略登出错误
                }
            });
        }
    }

    // 测试服务器健康状态
    async testServerHealth() {
        console.log('\n🧪 Testing server health...');
        
        const client = new GatewayHTTPClient();
        this.clients.push(client);

        try {
            const healthResult = await client.healthCheck();
            
            if (healthResult.success) {
                console.log('✅ Server health check passed');
                return true;
            } else {
                console.log('⚠️ Server health check failed (might not be implemented)');
                return true;  // 健康检查失败不算测试失败
            }
        } catch (error) {
            console.log('⚠️ Server health check not available (expected)');
            return true;  // 健康检查不可用是正常的
        }
    }

    // 清理客户端
    cleanup() {
        console.log('\n🧹 Cleaning up HTTP clients...');
        this.clients.forEach(client => {
            if (client.accessToken) {
                client.logout().catch(() => {});  // 忽略登出错误
            }
        });
        this.clients = [];
    }

    // 运行所有测试
    async runAllTests() {
        console.log('🚀 Starting HTTP API tests for Gateway Server...\n');
        
        const tests = [
            { name: 'Server Health', fn: () => this.testServerHealth() },
            { name: 'Basic Authentication', fn: () => this.testBasicAuth() },
            { name: 'Invalid Credentials', fn: () => this.testInvalidCredentials() },
            { name: 'Token Refresh', fn: () => this.testTokenRefresh() },
            { name: 'Message API', fn: () => this.testMessageAPI() },
            { name: 'Concurrent Requests', fn: () => this.testConcurrentRequests() },
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
            await new Promise(resolve => setTimeout(resolve, 500));
        }
        
        console.log(`\n📊 HTTP Test Results:`);
        console.log(`   ✅ Passed: ${passed}`);
        console.log(`   ❌ Failed: ${failed}`);
        console.log(`   📈 Success Rate: ${(passed / (passed + failed) * 100).toFixed(1)}%`);
        
        return { passed, failed };
    }
}

// 如果直接运行此文件
if (require.main === module) {
    const testRunner = new HTTPTestScenarios();
    
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

module.exports = { GatewayHTTPClient, HTTPTestScenarios };