#include "simple_message.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace im {
namespace gateway {

void SimpleMessage::print() const {
    std::cout << "\n=== 消息信息 ===" << std::endl;
    std::cout << "命令ID: " << cmd_id << std::endl;
    std::cout << "服务名称: " << service_name << std::endl;
    std::cout << "会话ID: " << session_id << std::endl;
    std::cout << "HTTP方法: " << method << std::endl;
    std::cout << "请求路径: " << path << std::endl;
    std::cout << "Token: " << (token.empty() ? "无" : token.substr(0, 10) + "...") << std::endl;
    std::cout << "设备ID: " << device_id << std::endl;
    std::cout << "平台: " << platform << std::endl;
    std::cout << "客户端IP: " << client_ip << std::endl;
    
    if (!body.empty()) {
        std::cout << "请求体: " << (body.length() > 100 ? body.substr(0, 100) + "..." : body) << std::endl;
    }
    
    // 格式化时间戳
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::cout << "时间戳: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
    std::cout << "===============" << std::endl;
}

SimpleResponse SimpleResponse::create_success(const std::string& data) {
    SimpleResponse response;
    response.status_code = 200;
    response.content_type = "application/json";
    response.body = data;
    response.success = true;
    return response;
}

SimpleResponse SimpleResponse::create_error(int code, const std::string& message) {
    SimpleResponse response;
    response.status_code = code;
    response.content_type = "application/json";
    response.success = false;
    response.error_message = message;
    
    // 构造错误响应JSON
    response.body = R"({"error_code": )" + std::to_string(code) + 
                   R"(, "error_message": ")" + message + R"("})";
    
    return response;
}

}  // namespace gateway
}  // namespace im