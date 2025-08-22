#include "simple_processor.hpp"
#include <iostream>
#include <sstream>
#include <thread>

namespace im {
namespace gateway {

// 静态成员初始化
int SimpleProcessor::session_counter_ = 0;

SimpleProcessor::SimpleProcessor() {
    // 默认的异步处理函数（模拟业务处理）
    async_handler_ = [](const SimpleMessage& msg) -> SimpleResponse {
        std::cout << "\n=== 开始异步处理消息 ===" << std::endl;
        msg.print();
        
        // 模拟异步处理（延时1秒）
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 根据不同的CMD_ID返回不同的响应
        std::string response_data;
        switch (msg.cmd_id) {
            case 1001:  // 登录
                response_data = R"({"status": "success", "message": "登录成功", "user_id": "12345"})";
                break;
            case 1002:  // 登出
                response_data = R"({"status": "success", "message": "登出成功"})";
                break;
            case 2001:  // 发送消息
                response_data = R"({"status": "success", "message": "消息发送成功", "message_id": "msg_67890"})";
                break;
            case 3003:  // 好友列表
                response_data = R"({"status": "success", "friends": [{"id": "friend1", "name": "张三"}, {"id": "friend2", "name": "李四"}]})";
                break;
            default:
                response_data = R"({"status": "success", "message": "处理完成"})";
        }
        
        std::cout << "异步处理完成，返回: " << response_data << std::endl;
        std::cout << "========================" << std::endl;
        
        return SimpleResponse::create_success(response_data);
    };
}

bool SimpleProcessor::load_config(const std::string& config_file) {
    std::cout << "加载配置文件: " << config_file << std::endl;
    return router_.load_config(config_file);
}

void SimpleProcessor::set_async_handler(AsyncHandler handler) {
    async_handler_ = handler;
}

void SimpleProcessor::handle_request(const httplib::Request& req, httplib::Response& res) {
    std::cout << "\n=== 收到HTTP请求 ===" << std::endl;
    std::cout << "方法: " << req.method << std::endl;
    std::cout << "路径: " << req.path << std::endl;
    
    // 1. 路由匹配
    auto route_info = router_.find_route(req.path);
    if (!route_info.found) {
        std::cout << "路由匹配失败" << std::endl;
        auto error_response = SimpleResponse::create_error(404, "路由未找到");
        res.status = error_response.status_code;
        res.set_content(error_response.body, error_response.content_type);
        return;
    }

    // 2. 提取请求信息，封装成SimpleMessage
    auto message = extract_message_from_request(req, route_info);
    
    // 3. 异步处理（使用std::async）
    std::cout << "投递到异步处理器..." << std::endl;
    auto future = std::async(std::launch::async, async_handler_, message);
    
    // 4. 等待异步处理结果
    auto response = future.get();
    
    // 5. 返回HTTP响应
    res.status = response.status_code;
    res.set_content(response.body, response.content_type);
    
    std::cout << "HTTP响应已发送" << std::endl;
    std::cout << "==================" << std::endl;
}

SimpleMessage SimpleProcessor::extract_message_from_request(const httplib::Request& req, 
                                                           const SimpleRouter::RouteInfo& route_info) {
    SimpleMessage message;
    
    // 基本信息
    message.cmd_id = route_info.cmd_id;
    message.service_name = route_info.service_name;
    message.session_id = generate_session_id();
    
    // HTTP信息
    message.method = req.method;
    message.path = req.path;
    message.body = req.body;
    
    // 认证信息
    message.token = extract_token(req);
    message.device_id = extract_device_id(req);
    message.platform = extract_platform(req);
    
    // 其他信息
    message.client_ip = extract_client_ip(req);
    message.timestamp = std::chrono::system_clock::now();
    
    return message;
}

std::string SimpleProcessor::extract_token(const httplib::Request& req) {
    // 从Authorization header提取
    auto auth_it = req.headers.find("Authorization");
    if (auth_it != req.headers.end()) {
        const std::string& auth_value = auth_it->second;
        if (auth_value.find("Bearer ") == 0) {
            return auth_value.substr(7);  // 去掉"Bearer "
        }
        return auth_value;
    }
    return "";
}

std::string SimpleProcessor::extract_device_id(const httplib::Request& req) {
    // 从X-Device-ID header提取
    auto device_it = req.headers.find("X-Device-ID");
    if (device_it != req.headers.end()) {
        return device_it->second;
    }
    
    // 从查询参数提取
    auto param_it = req.params.find("device_id");
    if (param_it != req.params.end()) {
        return param_it->second;
    }
    
    return "";
}

std::string SimpleProcessor::extract_platform(const httplib::Request& req) {
    // 从X-Platform header提取
    auto platform_it = req.headers.find("X-Platform");
    if (platform_it != req.headers.end()) {
        return platform_it->second;
    }
    
    // 从查询参数提取
    auto param_it = req.params.find("platform");
    if (param_it != req.params.end()) {
        return param_it->second;
    }
    
    return "unknown";
}

std::string SimpleProcessor::extract_client_ip(const httplib::Request& req) {
    // 尝试从X-Forwarded-For header提取
    auto forwarded_it = req.headers.find("X-Forwarded-For");
    if (forwarded_it != req.headers.end()) {
        return forwarded_it->second;
    }
    
    // 尝试从X-Real-IP header提取
    auto real_ip_it = req.headers.find("X-Real-IP");
    if (real_ip_it != req.headers.end()) {
        return real_ip_it->second;
    }
    
    return "unknown";
}

std::string SimpleProcessor::generate_session_id() {
    return "session_" + std::to_string(++session_counter_);
}

}  // namespace gateway
}  // namespace im