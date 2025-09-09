const { HTTPTestScenarios } = require('./http_client');
const { WebSocketTestScenarios } = require('./websocket_client');

class IntegratedTestRunner {
    constructor() {
        this.httpTester = new HTTPTestScenarios();
        this.wsTester = new WebSocketTestScenarios();
    }

    async runAllTests() {
        console.log('🚀 Starting Integrated Gateway Server Tests...\n');
        console.log('=' .repeat(60));
        
        let totalPassed = 0;
        let totalFailed = 0;
        
        try {
            // 运行HTTP测试
            console.log('\n📡 PHASE 1: HTTP API Tests');
            console.log('-'.repeat(40));
            const httpResults = await this.httpTester.runAllTests();
            totalPassed += httpResults.passed;
            totalFailed += httpResults.failed;
            
            // 等待一下让服务器稳定
            console.log('\n⏰ Waiting for server to stabilize...');
            await new Promise(resolve => setTimeout(resolve, 2000));
            
            // 运行WebSocket测试
            console.log('\n🔌 PHASE 2: WebSocket Tests');
            console.log('-'.repeat(40));
            const wsResults = await this.wsTester.runAllTests();
            totalPassed += wsResults.passed;
            totalFailed += wsResults.failed;
            
        } catch (error) {
            console.error('❌ Test runner encountered an error:', error.message);
            totalFailed++;
        } finally {
            // 清理
            this.cleanup();
        }
        
        // 最终结果
        console.log('\n' + '='.repeat(60));
        console.log('🏁 FINAL TEST RESULTS');
        console.log('='.repeat(60));
        console.log(`📊 Total Tests: ${totalPassed + totalFailed}`);
        console.log(`✅ Passed: ${totalPassed}`);
        console.log(`❌ Failed: ${totalFailed}`);
        console.log(`📈 Overall Success Rate: ${(totalPassed / (totalPassed + totalFailed) * 100).toFixed(1)}%`);
        
        if (totalFailed === 0) {
            console.log('\n🎉 ALL TESTS PASSED! Gateway Server is working perfectly! 🎉');
        } else if (totalFailed <= totalPassed * 0.2) {  // 失败率低于20%
            console.log('\n✅ Most tests passed. Gateway Server is working well with minor issues.');
        } else {
            console.log('\n⚠️ Some tests failed. Gateway Server needs attention.');
        }
        
        return {
            passed: totalPassed,
            failed: totalFailed,
            success: totalFailed === 0
        };
    }

    cleanup() {
        console.log('\n🧹 Final cleanup...');
        try {
            this.httpTester.cleanup();
            this.wsTester.cleanup();
        } catch (error) {
            console.warn('Warning during cleanup:', error.message);
        }
    }
}

// 性能测试运行器
class PerformanceTestRunner {
    constructor() {
        this.testResults = [];
    }

    async runPerformanceTests() {
        console.log('⚡ Starting Performance Tests...\n');
        
        // 吞吐量测试
        await this.testThroughput();
        
        // 延迟测试
        await this.testLatency();
        
        // 并发连接测试
        await this.testConcurrentConnections();
        
        // 生成报告
        this.generatePerformanceReport();
    }

    async testThroughput() {
        console.log('📈 Testing HTTP API throughput...');
        
        const { HTTPTestScenarios } = require('./http_client');
        const httpTester = new HTTPTestScenarios();
        
        const requests = 100;
        const concurrency = 10;
        
        const startTime = Date.now();
        
        // 创建并发请求批次
        const batches = [];
        for (let i = 0; i < concurrency; i++) {
            batches.push(this.createRequestBatch(requests / concurrency));
        }
        
        try {
            await Promise.all(batches);
            const endTime = Date.now();
            const duration = endTime - startTime;
            const throughput = (requests / duration) * 1000; // requests per second
            
            console.log(`✅ Throughput: ${throughput.toFixed(2)} requests/second`);
            
            this.testResults.push({
                test: 'HTTP Throughput',
                metric: 'requests/second',
                value: throughput.toFixed(2)
            });
        } catch (error) {
            console.error('❌ Throughput test failed:', error.message);
        }
        
        httpTester.cleanup();
    }

    async createRequestBatch(count) {
        const { GatewayHTTPClient } = require('./http_client');
        const client = new GatewayHTTPClient();
        
        const promises = [];
        for (let i = 0; i < count; i++) {
            promises.push(client.login(`perfuser${i}`, 'testpass'));
        }
        
        const results = await Promise.allSettled(promises);
        const successRate = results.filter(r => r.status === 'fulfilled' && r.value.success).length / count;
        
        return successRate;
    }

    async testLatency() {
        console.log('⏱️ Testing request latency...');
        
        const { GatewayHTTPClient } = require('./http_client');
        const client = new GatewayHTTPClient();
        
        const samples = 50;
        const latencies = [];
        
        try {
            for (let i = 0; i < samples; i++) {
                const startTime = Date.now();
                await client.login(`latencyuser${i}`, 'testpass');
                const endTime = Date.now();
                latencies.push(endTime - startTime);
                
                // 小延迟避免过度压力
                await new Promise(resolve => setTimeout(resolve, 50));
            }
            
            const avgLatency = latencies.reduce((sum, lat) => sum + lat, 0) / latencies.length;
            const minLatency = Math.min(...latencies);
            const maxLatency = Math.max(...latencies);
            
            console.log(`✅ Average latency: ${avgLatency.toFixed(2)}ms`);
            console.log(`   Min: ${minLatency}ms, Max: ${maxLatency}ms`);
            
            this.testResults.push(
                { test: 'Average Latency', metric: 'milliseconds', value: avgLatency.toFixed(2) },
                { test: 'Min Latency', metric: 'milliseconds', value: minLatency },
                { test: 'Max Latency', metric: 'milliseconds', value: maxLatency }
            );
        } catch (error) {
            console.error('❌ Latency test failed:', error.message);
        }
    }

    async testConcurrentConnections() {
        console.log('🔗 Testing concurrent connections capacity...');
        
        const { GatewayWebSocketClient } = require('./websocket_client');
        const maxConnections = 50;
        const clients = [];
        
        try {
            let successfulConnections = 0;
            
            // 尝试建立多个并发连接
            const connectionPromises = [];
            for (let i = 0; i < maxConnections; i++) {
                const client = new GatewayWebSocketClient();
                clients.push(client);
                
                connectionPromises.push((async () => {
                    try {
                        await client.login(`concuser${i}`, 'testpass');
                        await client.connectWebSocket();
                        return true;
                    } catch (error) {
                        return false;
                    }
                })());
            }
            
            const results = await Promise.allSettled(connectionPromises);
            successfulConnections = results.filter(r => r.status === 'fulfilled' && r.value).length;
            
            console.log(`✅ Successful concurrent connections: ${successfulConnections}/${maxConnections}`);
            
            this.testResults.push({
                test: 'Concurrent Connections',
                metric: 'connections',
                value: successfulConnections
            });
            
        } catch (error) {
            console.error('❌ Concurrent connections test failed:', error.message);
        } finally {
            // 清理所有客户端
            clients.forEach(client => client.disconnect());
        }
    }

    generatePerformanceReport() {
        console.log('\n📋 Performance Test Report');
        console.log('='.repeat(50));
        
        this.testResults.forEach(result => {
            console.log(`${result.test}: ${result.value} ${result.metric}`);
        });
        
        console.log('\n💡 Performance Recommendations:');
        
        // 基于结果给出建议
        const throughputResult = this.testResults.find(r => r.test === 'HTTP Throughput');
        if (throughputResult && parseFloat(throughputResult.value) < 100) {
            console.log('   • Consider optimizing HTTP request processing');
        }
        
        const avgLatencyResult = this.testResults.find(r => r.test === 'Average Latency');
        if (avgLatencyResult && parseFloat(avgLatencyResult.value) > 100) {
            console.log('   • High latency detected, check network and processing efficiency');
        }
        
        const connectionsResult = this.testResults.find(r => r.test === 'Concurrent Connections');
        if (connectionsResult && parseInt(connectionsResult.value) < 40) {
            console.log('   • Consider increasing connection pool size or optimizing connection handling');
        }
        
        if (this.testResults.length === 0) {
            console.log('   • No performance issues detected, system is performing well');
        }
    }
}

// 主函数
async function main() {
    console.log('🎯 Gateway Server Test Suite');
    console.log('🕐 ' + new Date().toISOString());
    console.log('='.repeat(60));
    
    const args = process.argv.slice(2);
    const mode = args[0] || 'all';
    
    try {
        if (mode === 'http') {
            const httpRunner = new HTTPTestScenarios();
            await httpRunner.runAllTests();
        } else if (mode === 'websocket' || mode === 'ws') {
            const wsRunner = new WebSocketTestScenarios();
            await wsRunner.runAllTests();
        } else if (mode === 'performance' || mode === 'perf') {
            const perfRunner = new PerformanceTestRunner();
            await perfRunner.runPerformanceTests();
        } else if (mode === 'all' || mode === 'integrated') {
            const integratedRunner = new IntegratedTestRunner();
            const results = await integratedRunner.runAllTests();
            
            // 如果需要，也运行性能测试
            if (results.success && args.includes('--with-performance')) {
                console.log('\n⚡ Running performance tests...');
                const perfRunner = new PerformanceTestRunner();
                await perfRunner.runPerformanceTests();
            }
            
            process.exit(results.success ? 0 : 1);
        } else {
            console.log('Usage: node test_runner.js [http|websocket|performance|all] [--with-performance]');
            console.log('  http        - Run only HTTP API tests');
            console.log('  websocket   - Run only WebSocket tests');
            console.log('  performance - Run only performance tests');
            console.log('  all         - Run all functional tests (default)');
            console.log('  --with-performance - Also run performance tests after functional tests');
            process.exit(1);
        }
    } catch (error) {
        console.error('💥 Test runner crashed:', error);
        process.exit(1);
    }
}

// 优雅退出处理
process.on('SIGINT', () => {
    console.log('\n👋 Test interrupted by user');
    process.exit(130);
});

process.on('uncaughtException', (error) => {
    console.error('💥 Uncaught exception:', error);
    process.exit(1);
});

// 运行主函数
if (require.main === module) {
    main();
}

module.exports = { IntegratedTestRunner, PerformanceTestRunner };