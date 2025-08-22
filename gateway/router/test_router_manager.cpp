#include "router_mgr.hpp"
#include <iostream>

using namespace im::gateway;

void test_router_manager() {
    std::cout << "=== RouterManager 测试 ===" << std::endl;
    
    // 创建RouterManager
    RouterManager router_manager("./config/config.json");
    
    // 获取统计信息
    auto stats = router_manager.get_stats();
    std::cout << "配置文件: " << stats.config_file << std::endl;
    std::cout << "HTTP路由数量: " << stats.http_route_count << std::endl;
    std::cout << "服务数量: " << stats.service_count << std::endl;
    
    std::cout << "\n=== 测试HTTP路由解析 ===" << std::endl;
    
    // 测试HTTP路由
    std::vector<std::pair<std::string, std::string>> test_routes = {
        {"POST", "/api/v1/auth/login"},
        {"POST", "/api/v1/auth/logout"},
        {"POST", "/api/v1/message/send"},
        {"GET", "/api/v1/friend/list"},
        {"GET", "/api/v1/unknown/path"}  // 应该失败
    };
    
    for (const auto& [method, path] : test_routes) {
        auto result = router_manager.parse_http_route(method, path);
        if (result && result->is_valid) {
            std::cout << "✓ " << method << " " << path 
                      << " -> CMD_ID: " << result->cmd_id 
                      << ", Service: " << result->service_name << std::endl;
        } else {
            std::cout << "✗ " << method << " " << path 
                      << " -> 失败: " << (result ? result->err_msg : "解析错误") << std::endl;
        }
    }
    
    std::cout << "\n=== 测试服务路由查找 ===" << std::endl;
    
    // 测试服务查找
    std::vector<std::string> test_services = {
        "user_service",
        "message_service", 
        "friend_service",
        "unknown_service"  // 应该失败
    };
    
    for (const auto& service_name : test_services) {
        auto result = router_manager.find_service(service_name);
        if (result && result->is_valid) {
            std::cout << "✓ 服务: " << service_name 
                      << " -> 端点: " << result->endpoint
                      << ", 超时: " << result->timeout_ms << "ms"
                      << ", 最大连接: " << result->max_connections << std::endl;
        } else {
            std::cout << "✗ 服务: " << service_name 
                      << " -> 失败: " << (result ? result->err_msg : "查找错误") << std::endl;
        }
    }
    
    std::cout << "\n=== 测试完整路由 ===" << std::endl;
    
    // 测试完整路由（HTTP路径 -> 命令ID + 服务端点）
    for (const auto& [method, path] : test_routes) {
        if (path.find("unknown") != std::string::npos) continue;  // 跳过已知会失败的
        
        auto result = router_manager.route_request(method, path);
        if (result && result->is_valid) {
            std::cout << "✓ " << method << " " << path << std::endl;
            std::cout << "    CMD_ID: " << result->cmd_id << std::endl;
            std::cout << "    服务名: " << result->service_name << std::endl;
            std::cout << "    端点: " << result->endpoint << std::endl;
            std::cout << "    超时: " << result->timeout_ms << "ms" << std::endl;
            std::cout << "    最大连接: " << result->max_connections << std::endl;
        } else {
            std::cout << "✗ " << method << " " << path 
                      << " -> 完整路由失败: " << (result ? result->err_msg : "路由错误") << std::endl;
        }
    }
    
    std::cout << "\n=== 测试配置重新加载 ===" << std::endl;
    
    if (router_manager.reload_config()) {
        std::cout << "✓ 配置重新加载成功" << std::endl;
    } else {
        std::cout << "✗ 配置重新加载失败" << std::endl;
    }
}

void demonstrate_usage() {
    std::cout << "\n=== RouterManager 使用示例 ===" << std::endl;
    
    RouterManager router("config.json");
    
    // 模拟一个完整的请求处理流程
    std::string method = "POST";
    std::string path = "/api/v1/auth/login";
    
    std::cout << "处理请求: " << method << " " << path << std::endl;
    
    // 第一步：完整路由解析
    auto route_result = router.route_request(method, path);
    if (!route_result || !route_result->is_valid) {
        std::cout << "路由失败: " << (route_result ? route_result->err_msg : "未知错误") << std::endl;
        return;
    }
    
    std::cout << "路由成功!" << std::endl;
    std::cout << "  命令ID: " << route_result->cmd_id << std::endl;
    std::cout << "  目标服务: " << route_result->service_name << std::endl;
    std::cout << "  服务端点: " << route_result->endpoint << std::endl;
    std::cout << "  连接超时: " << route_result->timeout_ms << "ms" << std::endl;
    
    // 这里可以继续进行业务处理：
    // 1. 根据cmd_id确定具体的处理逻辑
    // 2. 使用endpoint连接到微服务
    // 3. 使用timeout_ms设置连接超时
    // 4. 使用max_connections进行连接池管理
    
    std::cout << "\n接下来可以:" << std::endl;
    std::cout << "1. 根据CMD_ID(" << route_result->cmd_id << ")调用对应的业务处理器" << std::endl;
    std::cout << "2. 连接到微服务端点: " << route_result->endpoint << std::endl;
    std::cout << "3. 发送请求并等待响应" << std::endl;
    std::cout << "4. 返回处理结果给客户端" << std::endl;
}

int main() {
    try {
        test_router_manager();
        demonstrate_usage();
        
        std::cout << "\n=== 测试完成 ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}