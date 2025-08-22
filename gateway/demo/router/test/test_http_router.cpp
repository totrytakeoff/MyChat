#include "../http_router.hpp"
#include <iostream>
#include <cassert>

using namespace im::gateway;

void test_basic_routing() {
    std::cout << "Testing basic routing...\n";
    
    HttpRouter router;
    
    // 构造测试配置
    nlohmann::json config = {
        {"api_prefix", "/api/v1"},
        {"routes", {
            {
                {"pattern", "/auth/login"},
                {"methods", {"POST"}},
                {"cmd_id", 1001},
                {"service", "user_service"}
            },
            {
                {"pattern", "/message/send"},
                {"methods", {"POST"}},
                {"cmd_id", 2001},
                {"service", "message_service"}
            },
            {
                {"pattern", "/user/{user_id}"},
                {"methods", {"GET"}},
                {"cmd_id", 1004},
                {"service", "user_service"}
            }
        }}
    };
    
    bool loaded = router.load_config_from_json(config);
    assert(loaded && "Config loading failed");
    
    // 测试精确匹配
    auto result1 = router.parse_route("POST", "/api/v1/auth/login");
    assert(result1 && result1->is_valid && "Login route should match");
    assert(result1->cmd_id == 1001 && "Wrong command ID for login");
    assert(result1->service_name == "user_service" && "Wrong service for login");
    
    // 测试参数提取
    auto result2 = router.parse_route("GET", "/api/v1/user/123");
    assert(result2 && result2->is_valid && "User route should match");
    assert(result2->cmd_id == 1004 && "Wrong command ID for user");
    assert(result2->path_params.at("user_id") == "123" && "Wrong user_id parameter");
    
    // 测试方法不匹配
    auto result3 = router.parse_route("GET", "/api/v1/auth/login");
    assert(result3 && !result3->is_valid && "Login should not match GET method");
    
    // 测试路径不匹配
    auto result4 = router.parse_route("POST", "/api/v1/unknown/path");
    assert(result4 && !result4->is_valid && "Unknown path should not match");
    
    std::cout << "✓ Basic routing tests passed\n";
}

void test_path_parameters() {
    std::cout << "Testing path parameters...\n";
    
    HttpRouter router;
    
    nlohmann::json config = {
        {"api_prefix", "/api/v1"},
        {"routes", {
            {
                {"pattern", "/user/{user_id}/posts/{post_id}"},
                {"methods", {"GET"}},
                {"cmd_id", 5001},
                {"service", "content_service"}
            },
            {
                {"pattern", "/group/{group_id}/members/{user_id}"},
                {"methods", {"DELETE"}},
                {"cmd_id", 4006},
                {"service", "group_service"}
            }
        }}
    };
    
    router.load_config_from_json(config);
    
    // 测试多个参数
    auto result1 = router.parse_route("GET", "/api/v1/user/456/posts/789");
    assert(result1 && result1->is_valid && "Multi-parameter route should match");
    assert(result1->path_params.at("user_id") == "456" && "Wrong user_id");
    assert(result1->path_params.at("post_id") == "789" && "Wrong post_id");
    
    // 测试嵌套参数
    auto result2 = router.parse_route("DELETE", "/api/v1/group/100/members/200");
    assert(result2 && result2->is_valid && "Nested parameter route should match");
    assert(result2->path_params.at("group_id") == "100" && "Wrong group_id");
    assert(result2->path_params.at("user_id") == "200" && "Wrong user_id in group context");
    
    std::cout << "✓ Path parameter tests passed\n";
}

void test_priority_and_methods() {
    std::cout << "Testing priority and methods...\n";
    
    HttpRouter router;
    
    nlohmann::json config = {
        {"api_prefix", "/api/v1"},
        {"routes", {
            {
                {"pattern", "/user/info"},
                {"methods", {"GET"}},
                {"cmd_id", 1004},
                {"service", "user_service"},
                {"priority", 1}
            },
            {
                {"pattern", "/user/{user_id}"},
                {"methods", {"GET"}},
                {"cmd_id", 1006},
                {"service", "user_service"},
                {"priority", 2}  // 较低优先级
            },
            {
                {"pattern", "/message/send"},
                {"methods", {"POST", "PUT"}},
                {"cmd_id", 2001},
                {"service", "message_service"}
            }
        }}
    };
    
    router.load_config_from_json(config);
    
    // 测试优先级：精确匹配应该优先于参数匹配
    auto result1 = router.parse_route("GET", "/api/v1/user/info");
    assert(result1 && result1->is_valid && "Exact match should work");
    assert(result1->cmd_id == 1004 && "Should match exact route, not parameter route");
    
    // 测试多方法支持
    auto result2 = router.parse_route("POST", "/api/v1/message/send");
    assert(result2 && result2->is_valid && "POST method should match");
    
    auto result3 = router.parse_route("PUT", "/api/v1/message/send");
    assert(result3 && result3->is_valid && "PUT method should also match");
    
    auto result4 = router.parse_route("DELETE", "/api/v1/message/send");
    assert(result4 && !result4->is_valid && "DELETE method should not match");
    
    std::cout << "✓ Priority and methods tests passed\n";
}

void test_cache_functionality() {
    std::cout << "Testing cache functionality...\n";
    
    HttpRouter router;
    
    nlohmann::json config = {
        {"api_prefix", "/api/v1"},
        {"cache_config", {
            {"max_size", 10},
            {"ttl_minutes", 1}
        }},
        {"routes", {
            {
                {"pattern", "/test/cache"},
                {"methods", {"GET"}},
                {"cmd_id", 9999},
                {"service", "test_service"}
            }
        }}
    };
    
    router.load_config_from_json(config);
    
    // 第一次请求
    auto result1 = router.parse_route("GET", "/api/v1/test/cache");
    assert(result1 && result1->is_valid && "First request should work");
    
    // 第二次请求（应该命中缓存）
    auto result2 = router.parse_route("GET", "/api/v1/test/cache");
    assert(result2 && result2->is_valid && "Second request should work");
    
    // 检查缓存统计
    auto stats = router.get_cache_stats();
    assert(stats["cache_hits"] >= 1 && "Should have cache hits");
    assert(stats["cache_misses"] >= 1 && "Should have cache misses");
    
    std::cout << "✓ Cache functionality tests passed\n";
}

void test_path_normalization() {
    std::cout << "Testing path normalization...\n";
    
    HttpRouter router;
    
    nlohmann::json config = {
        {"api_prefix", "/api/v1"},
        {"routes", {
            {
                {"pattern", "/test/normalize"},
                {"methods", {"GET"}},
                {"cmd_id", 8888},
                {"service", "test_service"}
            }
        }}
    };
    
    router.load_config_from_json(config);
    
    // 测试各种路径格式
    std::vector<std::string> paths = {
        "/api/v1/test/normalize",
        "/api/v1/test/normalize/",
        "//api//v1//test//normalize//",
        "/api/v1/test/normalize////"
    };
    
    for (const auto& path : paths) {
        auto result = router.parse_route("GET", path);
        assert(result && result->is_valid && ("Path normalization failed for: " + path).c_str());
        assert(result->cmd_id == 8888 && "Wrong command ID after normalization");
    }
    
    std::cout << "✓ Path normalization tests passed\n";
}

void test_metadata_support() {
    std::cout << "Testing metadata support...\n";
    
    HttpRouter router;
    
    nlohmann::json config = {
        {"api_prefix", "/api/v1"},
        {"routes", {
            {
                {"pattern", "/auth/login"},
                {"methods", {"POST"}},
                {"cmd_id", 1001},
                {"service", "user_service"},
                {"metadata", {
                    {"auth_required", "false"},
                    {"rate_limit", "10/min"},
                    {"description", "用户登录接口"}
                }}
            }
        }}
    };
    
    router.load_config_from_json(config);
    
    auto result = router.parse_route("POST", "/api/v1/auth/login");
    assert(result && result->is_valid && "Login route should match");
    assert(result->metadata.at("auth_required") == "false" && "Wrong auth_required metadata");
    assert(result->metadata.at("rate_limit") == "10/min" && "Wrong rate_limit metadata");
    assert(result->metadata.at("description") == "用户登录接口" && "Wrong description metadata");
    
    std::cout << "✓ Metadata support tests passed\n";
}

void print_router_info(const HttpRouter& router) {
    std::cout << "\n=== Router Information ===\n";
    auto info = router.get_route_info();
    for (const auto& line : info) {
        std::cout << line << "\n";
    }
    
    auto stats = router.get_cache_stats();
    std::cout << "\n=== Cache Statistics ===\n";
    for (const auto& [key, value] : stats) {
        std::cout << key << ": " << value << "\n";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=== HttpRouter Test Suite ===\n\n";
    
    try {
        test_basic_routing();
        test_path_parameters();
        test_priority_and_methods();
        test_cache_functionality();
        test_path_normalization();
        test_metadata_support();
        
        std::cout << "\n=== All Tests Passed! ===\n";
        
        // 创建一个完整的路由器示例并打印信息
        std::cout << "\n=== Complete Router Example ===\n";
        HttpRouter example_router;
        if (example_router.load_config("../config/router_config.json")) {
            print_router_info(example_router);
            
            // 测试几个示例请求
            std::vector<std::pair<std::string, std::string>> test_requests = {
                {"POST", "/api/v1/auth/login"},
                {"GET", "/api/v1/user/123"},
                {"POST", "/api/v1/message/send"},
                {"GET", "/api/v1/friend/list"},
                {"DELETE", "/api/v1/group/456/kick/789"}
            };
            
            std::cout << "=== Example Requests ===\n";
            for (const auto& [method, path] : test_requests) {
                auto result = example_router.parse_route(method, path);
                if (result && result->is_valid) {
                    std::cout << method << " " << path 
                              << " -> CMD_ID: " << result->cmd_id 
                              << ", Service: " << result->service_name;
                    if (!result->path_params.empty()) {
                        std::cout << ", Params: {";
                        bool first = true;
                        for (const auto& [key, value] : result->path_params) {
                            if (!first) std::cout << ", ";
                            std::cout << key << "=" << value;
                            first = false;
                        }
                        std::cout << "}";
                    }
                    std::cout << "\n";
                } else {
                    std::cout << method << " " << path << " -> No match\n";
                }
            }
        } else {
            std::cout << "Failed to load example config file\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}