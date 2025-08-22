#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <filesystem>
#include "../../gateway/router/router_mgr.hpp"

using namespace im::gateway;
using namespace testing;

class RouterManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时配置文件
        config_file_path_ = "test_router_config.json";
        createTestConfig();
    }

    void TearDown() override {
        // 清理临时配置文件
        if (std::filesystem::exists(config_file_path_)) {
            std::filesystem::remove(config_file_path_);
        }
    }

    void createTestConfig() {
        nlohmann::json config = {
            {"http_router", {
                {"api_prefix", "/api/v1"},
                {"routes", {
                    {
                        {"path", "/auth/login"},
                        {"cmd_id", 1001},
                        {"service_name", "user_service"}
                    },
                    {
                        {"path", "/auth/logout"},
                        {"cmd_id", 1002},
                        {"service_name", "user_service"}
                    },
                    {
                        {"path", "/message/send"},
                        {"cmd_id", 2001},
                        {"service_name", "message_service"}
                    },
                    {
                        {"path", "/friend/list"},
                        {"cmd_id", 3003},
                        {"service_name", "friend_service"}
                    }
                }}
            }},
            {"service_router", {
                {"services", {
                    {
                        {"service_name", "user_service"},
                        {"endpoint", "localhost:8001"},
                        {"timeout_ms", 5000},
                        {"max_connections", 10}
                    },
                    {
                        {"service_name", "message_service"},
                        {"endpoint", "localhost:8002"},
                        {"timeout_ms", 3000},
                        {"max_connections", 20}
                    },
                    {
                        {"service_name", "friend_service"},
                        {"endpoint", "localhost:8003"},
                        {"timeout_ms", 3000},
                        {"max_connections", 15}
                    }
                }}
            }}
        };

        std::ofstream file(config_file_path_);
        file << config.dump(4);
        file.close();
    }

    void createInvalidConfig() {
        nlohmann::json invalid_config = {
            {"http_router", {
                {"api_prefix", "/api/v1"},
                {"routes", {
                    {
                        // 缺少必要字段
                        {"path", "/invalid/route"}
                        // 缺少cmd_id和service_name
                    }
                }}
            }},
            {"service_router", {
                {"services", {
                    {
                        // 缺少endpoint
                        {"service_name", "invalid_service"},
                        {"timeout_ms", 3000}
                    }
                }}
            }}
        };

        std::ofstream file(config_file_path_);
        file << invalid_config.dump(4);
        file.close();
    }

    std::string config_file_path_;
};

// ===== HttpRouter 测试 =====

TEST_F(RouterManagerTest, HttpRouter_ValidRoutes_ShouldParseSuccessfully) {
    RouterManager router_manager(config_file_path_);
    
    // 测试有效路由
    auto result = router_manager.parse_http_route("POST", "/api/v1/auth/login");
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->is_valid);
    EXPECT_EQ(result->cmd_id, 1001);
    EXPECT_EQ(result->service_name, "user_service");
    EXPECT_EQ(result->status_code, 200);
}

TEST_F(RouterManagerTest, HttpRouter_InvalidPath_ShouldFail) {
    RouterManager router_manager(config_file_path_);
    
    // 测试无效路径
    auto result = router_manager.parse_http_route("POST", "/api/v1/unknown/path");
    
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
    EXPECT_EQ(result->status_code, 404);
    EXPECT_THAT(result->err_msg, HasSubstr("Route not found"));
}

TEST_F(RouterManagerTest, HttpRouter_InvalidPrefix_ShouldFail) {
    RouterManager router_manager(config_file_path_);
    
    // 测试错误的API前缀
    auto result = router_manager.parse_http_route("POST", "/api/v2/auth/login");
    
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
    EXPECT_EQ(result->status_code, 404);
    EXPECT_THAT(result->err_msg, HasSubstr("Path does not match API prefix"));
}

TEST_F(RouterManagerTest, HttpRouter_MultipleRoutes_ShouldParseCorrectly) {
    RouterManager router_manager(config_file_path_);
    
    struct TestCase {
        std::string method;
        std::string path;
        uint32_t expected_cmd_id;
        std::string expected_service;
    };
    
    std::vector<TestCase> test_cases = {
        {"POST", "/api/v1/auth/login", 1001, "user_service"},
        {"POST", "/api/v1/auth/logout", 1002, "user_service"},
        {"POST", "/api/v1/message/send", 2001, "message_service"},
        {"GET", "/api/v1/friend/list", 3003, "friend_service"}
    };
    
    for (const auto& test_case : test_cases) {
        auto result = router_manager.parse_http_route(test_case.method, test_case.path);
        
        ASSERT_NE(result, nullptr) << "Failed for: " << test_case.method << " " << test_case.path;
        EXPECT_TRUE(result->is_valid) << "Route should be valid for: " << test_case.path;
        EXPECT_EQ(result->cmd_id, test_case.expected_cmd_id) << "Wrong CMD_ID for: " << test_case.path;
        EXPECT_EQ(result->service_name, test_case.expected_service) << "Wrong service for: " << test_case.path;
    }
}

// ===== ServiceRouter 测试 =====

TEST_F(RouterManagerTest, ServiceRouter_ValidService_ShouldFindSuccessfully) {
    RouterManager router_manager(config_file_path_);
    
    // 测试查找有效服务
    auto result = router_manager.find_service("user_service");
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->is_valid);
    EXPECT_EQ(result->service_name, "user_service");
    EXPECT_EQ(result->endpoint, "localhost:8001");
    EXPECT_EQ(result->timeout_ms, 5000);
    EXPECT_EQ(result->max_connections, 10);
}

TEST_F(RouterManagerTest, ServiceRouter_InvalidService_ShouldFail) {
    RouterManager router_manager(config_file_path_);
    
    // 测试查找不存在的服务
    auto result = router_manager.find_service("unknown_service");
    
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
    EXPECT_THAT(result->err_msg, HasSubstr("Service not found"));
}

TEST_F(RouterManagerTest, ServiceRouter_EmptyServiceName_ShouldFail) {
    RouterManager router_manager(config_file_path_);
    
    // 测试空服务名
    auto result = router_manager.find_service("");
    
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
    EXPECT_THAT(result->err_msg, HasSubstr("Service name is empty"));
}

TEST_F(RouterManagerTest, ServiceRouter_MultipleServices_ShouldFindCorrectly) {
    RouterManager router_manager(config_file_path_);
    
    struct ServiceTestCase {
        std::string service_name;
        std::string expected_endpoint;
        uint32_t expected_timeout;
        uint32_t expected_max_conn;
    };
    
    std::vector<ServiceTestCase> test_cases = {
        {"user_service", "localhost:8001", 5000, 10},
        {"message_service", "localhost:8002", 3000, 20},
        {"friend_service", "localhost:8003", 3000, 15}
    };
    
    for (const auto& test_case : test_cases) {
        auto result = router_manager.find_service(test_case.service_name);
        
        ASSERT_NE(result, nullptr) << "Failed for service: " << test_case.service_name;
        EXPECT_TRUE(result->is_valid) << "Service should be valid: " << test_case.service_name;
        EXPECT_EQ(result->endpoint, test_case.expected_endpoint);
        EXPECT_EQ(result->timeout_ms, test_case.expected_timeout);
        EXPECT_EQ(result->max_connections, test_case.expected_max_conn);
    }
}

// ===== RouterManager 完整路由测试 =====

TEST_F(RouterManagerTest, CompleteRoute_ValidRequest_ShouldRouteSuccessfully) {
    RouterManager router_manager(config_file_path_);
    
    // 测试完整路由
    auto result = router_manager.route_request("POST", "/api/v1/auth/login");
    
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->is_valid);
    EXPECT_EQ(result->cmd_id, 1001);
    EXPECT_EQ(result->service_name, "user_service");
    EXPECT_EQ(result->endpoint, "localhost:8001");
    EXPECT_EQ(result->timeout_ms, 5000);
    EXPECT_EQ(result->max_connections, 10);
    EXPECT_EQ(result->http_status_code, 200);
}

TEST_F(RouterManagerTest, CompleteRoute_InvalidHttpRoute_ShouldFail) {
    RouterManager router_manager(config_file_path_);
    
    // HTTP路由失败的情况
    auto result = router_manager.route_request("POST", "/api/v1/unknown/path");
    
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
    EXPECT_EQ(result->http_status_code, 404);
    EXPECT_THAT(result->err_msg, HasSubstr("Route not found"));
}

TEST_F(RouterManagerTest, CompleteRoute_ServiceNotFound_ShouldFail) {
    // 创建一个HTTP路由指向不存在服务的配置
    nlohmann::json config = {
        {"http_router", {
            {"api_prefix", "/api/v1"},
            {"routes", {
                {
                    {"path", "/test/route"},
                    {"cmd_id", 9999},
                    {"service_name", "nonexistent_service"}  // 不存在的服务
                }
            }}
        }},
        {"service_router", {
            {"services", {
                {
                    {"service_name", "user_service"},
                    {"endpoint", "localhost:8001"},
                    {"timeout_ms", 5000},
                    {"max_connections", 10}
                }
            }}
        }}
    };
    
    std::ofstream file(config_file_path_);
    file << config.dump(4);
    file.close();
    
    RouterManager router_manager(config_file_path_);
    
    // HTTP路由成功，但服务路由失败
    auto result = router_manager.route_request("POST", "/api/v1/test/route");
    
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
    EXPECT_EQ(result->http_status_code, 503);  // Service Unavailable
    EXPECT_THAT(result->err_msg, HasSubstr("Service not found"));
}

// ===== 配置管理测试 =====

TEST_F(RouterManagerTest, ConfigReload_ValidConfig_ShouldSucceed) {
    RouterManager router_manager(config_file_path_);
    
    // 重新加载配置
    EXPECT_TRUE(router_manager.reload_config());
    
    // 验证配置重新加载后仍然工作
    auto result = router_manager.route_request("POST", "/api/v1/auth/login");
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->is_valid);
}

TEST_F(RouterManagerTest, ConfigReload_InvalidConfig_ShouldFail) {
    RouterManager router_manager(config_file_path_);
    
    // 创建完全无效的JSON格式配置（这会导致解析失败）
    std::ofstream file(config_file_path_);
    file << "{ invalid json content without proper closing";
    file.close();
    
    // 重新加载应该失败
    EXPECT_FALSE(router_manager.reload_config());
}

TEST_F(RouterManagerTest, GetStats_ShouldReturnCorrectInfo) {
    RouterManager router_manager(config_file_path_);
    
    auto stats = router_manager.get_stats();
    
    EXPECT_EQ(stats.config_file, config_file_path_);
    EXPECT_EQ(stats.http_route_count, 4);  // 4个HTTP路由
    EXPECT_EQ(stats.service_count, 3);     // 3个服务
}

// ===== 错误处理测试 =====

TEST_F(RouterManagerTest, NonexistentConfigFile_ShouldHandleGracefully) {
    // 使用不存在的配置文件
    RouterManager router_manager("nonexistent_config.json");
    
    // 应该能创建，但路由会失败
    auto result = router_manager.route_request("POST", "/api/v1/auth/login");
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
}

TEST_F(RouterManagerTest, MalformedConfigFile_ShouldHandleGracefully) {
    // 创建格式错误的配置文件
    std::ofstream file(config_file_path_);
    file << "{ invalid json content";
    file.close();
    
    RouterManager router_manager(config_file_path_);
    
    // 应该能创建，但路由会失败
    auto result = router_manager.route_request("POST", "/api/v1/auth/login");
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
}

// ===== 性能测试 =====

TEST_F(RouterManagerTest, Performance_MultipleRouteLookups_ShouldBeEfficient) {
    RouterManager router_manager(config_file_path_);
    
    const int num_lookups = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_lookups; ++i) {
        auto result = router_manager.route_request("POST", "/api/v1/auth/login");
        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(result->is_valid);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 每次查找应该在合理时间内完成（这里设为平均100微秒，可根据需要调整）
    EXPECT_LT(duration.count() / num_lookups, 100) << "Route lookup too slow";
    
    std::cout << "Average lookup time: " << (duration.count() / num_lookups) << " microseconds" << std::endl;
}

// ===== 边界条件测试 =====

TEST_F(RouterManagerTest, EdgeCases_EmptyPath_ShouldHandleGracefully) {
    RouterManager router_manager(config_file_path_);
    
    auto result = router_manager.route_request("POST", "");
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
}

TEST_F(RouterManagerTest, EdgeCases_VeryLongPath_ShouldHandleGracefully) {
    RouterManager router_manager(config_file_path_);
    
    std::string very_long_path = "/api/v1/" + std::string(10000, 'a');
    auto result = router_manager.route_request("POST", very_long_path);
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_valid);
}

TEST_F(RouterManagerTest, EdgeCases_SpecialCharactersInPath_ShouldHandleGracefully) {
    RouterManager router_manager(config_file_path_);
    
    std::vector<std::string> special_paths = {
        "/api/v1/auth/login?param=value",
        "/api/v1/auth/login#fragment",
        "/api/v1/auth/login/../logout",
        "/api/v1/auth/login%20encoded"
    };
    
    for (const auto& path : special_paths) {
        auto result = router_manager.route_request("POST", path);
        ASSERT_NE(result, nullptr);
        // 这些路径应该都不匹配，因为我们的路由是精确匹配
        EXPECT_FALSE(result->is_valid);
    }
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}