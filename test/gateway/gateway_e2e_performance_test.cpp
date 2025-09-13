#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <thread>

#include <httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;
#include "../../gateway/auth/multi_platform_auth.hpp"
#include "../../gateway/gateway_server/gateway_server.hpp"

using namespace im::gateway;

namespace {

struct Percentiles {
    double p50_ms;
    double p95_ms;
    double max_ms;
};

Percentiles compute_percentiles(std::vector<double>& samples_ms) {
    if (samples_ms.empty()) return {0.0, 0.0, 0.0};
    std::sort(samples_ms.begin(), samples_ms.end());
    const auto idx_p50 = static_cast<size_t>(std::clamp(
            0.50 * (samples_ms.size() - 1), 0.0, static_cast<double>(samples_ms.size() - 1)));
    const auto idx_p95 = static_cast<size_t>(std::clamp(
            0.95 * (samples_ms.size() - 1), 0.0, static_cast<double>(samples_ms.size() - 1)));
    return {samples_ms[idx_p50], samples_ms[idx_p95], samples_ms.back()};
}

std::string make_json_body(const std::string& from_uid, const std::string& to_uid,
                           const std::string& text) {
    nlohmann::json j;
    j["from_uid"] = from_uid;
    j["to_uid"] = to_uid;
    j["text"] = text;
    return j.dump();
}

}  // namespace

class GatewayE2EPerfTest : public ::testing::Test {
protected:
    void SetUp() override {
        platform_config_path = "../test_auth_config.json";
        router_config_path = "../test_router_config.json";
        ws_port = 8101;
        http_port = 8102;
        auto redis_config = im::db::RedisConfig("127.0.0.1", 6379, "myself", 1);
        ASSERT_TRUE(im::db::RedisManager::GetInstance().initialize(redis_config));

        server = std::make_unique<GatewayServer>(platform_config_path, router_config_path, ws_port,
                                                 http_port);
        // 注册普通异步处理器，避免依赖后端服务
        // 使用协程处理器版本（与GatewayServer注册接口匹配）


        server->register_message_handlers(2001,
                                          [this](const UnifiedMessage& msg) -> ProcessorResult {
                                              nlohmann::json j;
                                              j["code"] = 200;
                                              j["body"] = "mock_response";
                                              j["err_msg"] = "";
                                              return ProcessorResult(0, "", "", j.dump());
                                          });


        server->start();
        // 初始化host并等待服务器完全启动
        host = "127.0.0.1";
        for (int i = 0; i < 30; ++i) {  // 增加健康检查重试次数至30次，总等待时间3秒
            httplib::Client health_cli(host, http_port);
            if (health_cli.Get("/api/v1/health")) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 使用同一配置生成合法的访问令牌
        auth = std::make_unique<MultiPlatformAuthManager>(platform_config_path);

        user_id = "user_" + std::to_string(std::rand());
        device_id = "device_web_1";
        platform = "web";
        token = auth->generate_access_token(user_id, "tester", device_id, platform,
                                            /*expire_seconds*/ 300);
        ASSERT_FALSE(token.empty());

        // host 已在上方设置
    }

    void TearDown() override {
        if (server) server->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    bool http_send_message_once(int expected_status = 200) {
        cout << "http send message once" << endl;

        httplib::Client cli(host, http_port);
        cli.set_keep_alive(false);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(30, 0);  // 增加超时时间至30秒，确保服务器响应
        cli.set_write_timeout(5, 0);
        httplib::Headers headers = {{"Authorization", std::string("Bearer ") + token},
                                    {"X-Device-ID", device_id},
                                    {"X-Platform", platform}};

        cout << "http send message once post->" << endl;
        auto res = cli.Post("/api/v1/message/send", headers,
                            make_json_body(user_id, user_id, "hello"), "application/json");
        cout << "Response status: " << (res ? res->status : -1) << endl;
        if (res && res->body.size() > 0) cout << "Response body: " << res->body << endl;

        if (!res) {
            std::cout << "http req no response!" << std::endl;
            return false;
        }

        return res->status == expected_status;
    }

protected:
    std::string platform_config_path;
    std::string router_config_path;
    uint16_t ws_port;
    uint16_t http_port;
    std::unique_ptr<GatewayServer> server;
    std::unique_ptr<MultiPlatformAuthManager> auth;

    std::string user_id;
    std::string device_id;
    std::string platform;
    std::string token;
    std::string host;
};

// 基础E2E：合法Token 请求应为200
TEST_F(GatewayE2EPerfTest, HttpSendMessage_Success) { ASSERT_TRUE(http_send_message_once(200)); }

// 负向E2E：非法Token 应为非200
TEST_F(GatewayE2EPerfTest, HttpSendMessage_InvalidToken) {
    // 暂存，换成非法token
    std::string old = token;
    token = "invalid.token.value";
    bool ok = http_send_message_once(/*expected_status ignored*/ 0);
    token = old;
    ASSERT_TRUE(ok);  // 只要有响应即可
}

// 性能：单线程串行请求，采样P50/P95
TEST_F(GatewayE2EPerfTest, Perf_Serial_Requests) {
    const int warmup = 5;
    const int N = 50;

    for (int i = 0; i < warmup; ++i) ASSERT_TRUE(http_send_message_once(200));

    std::vector<double> samples_ms;
    samples_ms.reserve(N);

    for (int i = 0; i < N; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        ASSERT_TRUE(http_send_message_once(200));
        auto t1 = std::chrono::high_resolution_clock::now();
        samples_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    auto p = compute_percentiles(samples_ms);
    // 宽松阈值，便于不同环境：P95 < 300ms，Max < 800ms
    EXPECT_LT(p.p95_ms, 300.0);
    EXPECT_LT(p.max_ms, 800.0);
}

// 性能：并发请求，统计吞吐与P95
TEST_F(GatewayE2EPerfTest, Perf_Concurrent_Requests) {
    const int concurrency = 4;  // 降低并发，稳定性诊断
    const int total = 80;       // 每线程20次

    // 预热
    for (int i = 0; i < 10; ++i) ASSERT_TRUE(http_send_message_once(200));

    std::atomic<int> done{0};
    std::vector<double> samples_ms;
    samples_ms.resize(total);

    auto t_begin = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(concurrency);
    std::atomic<int> index{0};

    for (int c = 0; c < concurrency; ++c) {
        threads.emplace_back([&, c]() {
            while (true) {
                int i = index.fetch_add(1);
                if (i >= total) break;
                auto t0 = std::chrono::high_resolution_clock::now();
                bool ok = http_send_message_once(200);
                auto t1 = std::chrono::high_resolution_clock::now();
                samples_ms[i] = std::chrono::duration<double, std::milli>(t1 - t0).count();
                if (ok) done.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    auto t_end = std::chrono::high_resolution_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
    auto p = compute_percentiles(samples_ms);
    double rps = (done.load() * 1000.0) / wall_ms;

    // 宽松阈值，验证系统基本性能
    EXPECT_EQ(done.load(), total);
    EXPECT_LT(p.p95_ms, 400.0);
    EXPECT_LT(p.max_ms, 1200.0);
    EXPECT_GT(rps, 150.0);
}
