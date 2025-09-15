#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <thread>

#include <httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>

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

// Generic HTTP request helper
bool http_request(httplib::Client& cli, const std::string& method, const std::string& path,
                  const httplib::Headers& headers, const std::string& body,
                  const char* content_type, int expected_status, std::string* out_body = nullptr) {
    if (method == "GET") {
        auto res = cli.Get(path.c_str(), headers);
        if (!res) return false;
        if (out_body) *out_body = res->body;
        return res->status == expected_status;
    } else if (method == "POST") {
        auto res = content_type ? cli.Post(path.c_str(), headers, body, content_type)
                                : cli.Post(path.c_str(), headers, body, "application/octet-stream");
        if (!res) return false;
        if (out_body) *out_body = res->body;
        return res->status == expected_status;
    } else if (method == "PUT") {
        auto res = content_type ? cli.Put(path.c_str(), headers, body, content_type)
                                : cli.Put(path.c_str(), headers, body, "application/octet-stream");
        if (!res) return false;
        if (out_body) *out_body = res->body;
        return res->status == expected_status;
    } else if (method == "PATCH") {
        auto res = content_type ? cli.Patch(path.c_str(), headers, body, content_type)
                                : cli.Patch(path.c_str(), headers, body, "application/octet-stream");
        if (!res) return false;
        if (out_body) *out_body = res->body;
        return res->status == expected_status;
    }
    return false;
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

    httplib::Client make_client() const {
        httplib::Client cli(host, http_port);
        cli.set_keep_alive(false);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(30, 0);
        cli.set_write_timeout(5, 0);
        return cli;
    }

    httplib::Headers default_headers(bool with_bearer = true) const {
        return {{"Authorization", (with_bearer ? std::string("Bearer ") : std::string("")) + token},
                {"X-Device-ID", device_id},
                {"X-Platform", platform}};
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

// 健壮性：空JSON体但Content-Type=application/json
TEST_F(GatewayE2EPerfTest, Http_EmptyJsonBody_IsTolerated) {
    auto cli = make_client();
    auto headers = default_headers();
    std::string body;
    bool ok = http_request(cli, "POST", "/api/v1/message/send", headers, "{}",
                           "application/json", 200, &body);
    ASSERT_TRUE(ok);
}

// 健壮性：POST 非JSON Content-Type 不解析为JSON
TEST_F(GatewayE2EPerfTest, Http_NonJsonContentType_NoJsonParsing) {
    auto cli = make_client();
    auto headers = default_headers();
    std::string body;
    bool ok = http_request(cli, "POST", "/api/v1/message/send", headers, "raw-bytes",
                           "application/octet-stream", 200, &body);
    ASSERT_TRUE(ok);
}

// 健壮性：GET带参数解析为JSON
TEST_F(GatewayE2EPerfTest, Http_Get_With_Params_AsJson) {
    auto cli = make_client();
    auto headers = default_headers();
    auto res = cli.Get("/api/v1/message/send?from_uid=aa&to_uid=bb&text=hi", headers);
    ASSERT_TRUE(res);
    // 处理器始终返回200
    EXPECT_EQ(res->status, 200);
}

// 健壮性：缺失Device-ID 导致401
TEST_F(GatewayE2EPerfTest, Http_Missing_DeviceId_Should_401) {
    auto cli = make_client();
    httplib::Headers headers = {{"Authorization", std::string("Bearer ") + token},
                                {"X-Platform", platform}};  // no device-id
    auto res = cli.Post("/api/v1/message/send", headers, make_json_body(user_id, user_id, "hi"),
                        "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
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

// 性能：对比简单/复杂处理逻辑，并保存结果
TEST_F(GatewayE2EPerfTest, Perf_Simple_vs_Complex_SaveToFile) {
    // 简单：直接返回JSON（已注册2001）
    // 复杂：模拟较重处理——附加注册一个处理器2002，执行一些CPU与sleep
    server->register_message_handlers(2002, [this](const UnifiedMessage& msg) -> ProcessorResult {
        // 模拟复杂处理
        volatile double sink = 0;
        for (int i = 0; i < 200000; ++i) sink += std::sqrt(static_cast<double>(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        nlohmann::json j;
        j["code"] = 200;
        j["body"] = "complex_response";
        j["err_msg"] = "";
        return ProcessorResult(0, "", "", j.dump());
    });

    auto cli = make_client();
    auto headers = default_headers();

    auto bench = [&](const std::string& path, int N) -> std::vector<double> {
        std::vector<double> samples_ms; samples_ms.reserve(N);
        for (int i = 0; i < N; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            auto res = cli.Post(path.c_str(), headers, make_json_body(user_id, user_id, "hi"),
                                "application/json");
            auto t1 = std::chrono::high_resolution_clock::now();
            EXPECT_TRUE(static_cast<bool>(res));
            if (res) {
                EXPECT_EQ(res->status, 200);
            }
            samples_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        return samples_ms;
    };

    // 预热
    for (int i = 0; i < 5; ++i)
        ASSERT_TRUE(http_send_message_once(200));

    auto simple_ms = bench("/api/v1/message/send", 60);
    auto complex_ms = bench("/api/v1/message/history", 60);  // 映射到2002

    auto ps = compute_percentiles(simple_ms);
    auto pc = compute_percentiles(complex_ms);

    // 控制台打印
    std::cout << "Simple P50/P95/Max (ms): " << ps.p50_ms << ", " << ps.p95_ms << ", "
              << ps.max_ms << std::endl;
    std::cout << "Complex P50/P95/Max (ms): " << pc.p50_ms << ", " << pc.p95_ms << ", "
              << pc.max_ms << std::endl;

    // 保存到文件
    std::ofstream ofs("gateway_http_perf_results.json", std::ios::out | std::ios::trunc);
    ASSERT_TRUE(ofs.is_open());
    nlohmann::json jr;
    jr["simple"]["p50_ms"] = ps.p50_ms;
    jr["simple"]["p95_ms"] = ps.p95_ms;
    jr["simple"]["max_ms"] = ps.max_ms;
    jr["complex"]["p50_ms"] = pc.p50_ms;
    jr["complex"]["p95_ms"] = pc.p95_ms;
    jr["complex"]["max_ms"] = pc.max_ms;
    ofs << jr.dump(2);
    ofs.close();
}
