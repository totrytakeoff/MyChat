
#include "processor.hpp"
#include <iostream>
#include <iomanip>

namespace im {
namespace gateway {

// 静态成员初始化
int SimpleHttpProcessor::session_counter_ = 0;


// ===== SimpleHttpProcessor 实现 =====

SimpleHttpProcessor::SimpleHttpProcessor() {
    http_router_ = nullptr;  // 延迟初始化
}

bool SimpleHttpProcessor::load_config(const std::string& config_file) {
    std::cout << "加载配置文件: " << config_file << std::endl;
    
    // 初始化HttpRouter（使用你现有的）
    http_router_ = std::make_unique<HttpRouter>(config_file);
    
    return http_router_->load_config(config_file);
}

std::unique_ptr<UnifiedMessage> SimpleHttpProcessor::process_http_request(
    const httplib::Request& req,
    const std::string& session_id) {
    
    std::cout << "\n=== 处理HTTP请求 ===" << std::endl;
    std::cout << "方法: " << req.method << ", 路径: " << req.path << std::endl;
    
    if (!http_router_) {
        std::cerr << "HttpRouter未初始化！" << std::endl;
        return nullptr;
    }
    
    // 1. 使用你现有的HttpRouter进行路由解析
    auto route_result = http_router_->parse_route(req.method, req.path);
    if (!route_result || !route_result->is_valid) {
        std::cout << "路由解析失败: " << (route_result ? route_result->err_msg : "未知错误") << std::endl;
        return nullptr;
    }
    
    std::cout << "路由匹配成功: CMD_ID=" << route_result->cmd_id 
              << ", Service=" << route_result->service_name << std::endl;
    
    // 2. 创建UnifiedMessage
    auto message = std::make_unique<UnifiedMessage>();
    
    // 3. 构造IMHeader（这是核心！）
    im::base::IMHeader header;
    header.set_version("1.0");
    header.set_seq(0);  // HTTP请求不需要序列号
    header.set_cmd_id(route_result->cmd_id);
    header.set_token(extract_token(req));
    header.set_device_id(extract_device_id(req));
    header.set_platform(extract_platform(req));
    
    // 设置时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    header.set_timestamp(static_cast<uint64_t>(timestamp_ms));
    
    message->set_header(std::move(header));
    
    // 4. 设置会话上下文
    UnifiedMessage::SessionContext context;
    context.protocol = UnifiedMessage::Protocol::HTTP;
    context.session_id = session_id.empty() ? generate_session_id() : session_id;
    context.client_ip = extract_client_ip(req);
    context.receive_time = now;
    context.http_method = req.method;
    context.original_path = req.path;
    context.raw_body = req.body;
    
    message->set_session_context(std::move(context));
    
    // 5. 设置消息体（保留JSON格式，便于后续处理）
    if (!req.body.empty()) {
        message->set_json_body(req.body);
    }
    
    std::cout << "HTTP请求处理完成" << std::endl;
    return message;
}

std::unique_ptr<UnifiedMessage> SimpleHttpProcessor::process_websocket_message(
    const std::string& raw_message,
    const std::string& session_id) {
    
    std::cout << "\n=== 处理WebSocket消息 ===" << std::endl;
    
    // 1. 使用现有的ProtobufCodec解码
    im::base::IMHeader header;
    
    // 临时消息对象用于解析
    im::base::BaseResponse temp_message;
    if (!protobuf_codec_.decode(raw_message, header, temp_message)) {
        std::cout << "Protobuf解码失败" << std::endl;
        return nullptr;
    }
    
    std::cout << "WebSocket消息解码成功: CMD_ID=" << header.cmd_id() << std::endl;
    
    // 2. 创建UnifiedMessage
    auto message = std::make_unique<UnifiedMessage>();
    
    // 3. 直接使用解码出的header（WebSocket的优势！）
    message->set_header(header);
    
    // 4. 设置会话上下文
    UnifiedMessage::SessionContext context;
    context.protocol = UnifiedMessage::Protocol::WEBSOCKET;
    context.session_id = session_id.empty() ? generate_session_id() : session_id;
    context.receive_time = std::chrono::system_clock::now();
    // WebSocket不需要HTTP相关字段
    
    message->set_session_context(std::move(context));
    
    // 5. 创建正确的protobuf消息对象（这里简化，实际应该根据cmd_id创建）
    auto protobuf_msg = std::make_unique<im::base::BaseResponse>(temp_message);
    message->set_protobuf_message(std::move(protobuf_msg));
    
    std::cout << "WebSocket消息处理完成" << std::endl;
    return message;
}

// ===== 辅助方法实现 =====

std::string SimpleHttpProcessor::extract_token(const httplib::Request& req) {
    auto auth_it = req.headers.find("Authorization");
    if (auth_it != req.headers.end()) {
        const std::string& auth_value = auth_it->second;
        if (auth_value.find("Bearer ") == 0) {
            return auth_value.substr(7);
        }
        return auth_value;
    }
    return "";
}

std::string SimpleHttpProcessor::extract_device_id(const httplib::Request& req) {
    auto device_header = req.headers.find("X-Device-ID");
    if (device_header != req.headers.end()) {
        return device_header->second;
    }
    auto device_param = req.params.find("device_id");
    if (device_param != req.params.end()) {
        return device_param->second;
    }
    return "";
}

std::string SimpleHttpProcessor::extract_platform(const httplib::Request& req) {
    auto platform_header = req.headers.find("X-Platform");
    if (platform_header != req.headers.end()) {
        return platform_header->second;
    }
    auto platform_param = req.params.find("platform");
    if (platform_param != req.params.end()) {
        return platform_param->second;
    }
    return "unknown";
}

std::string SimpleHttpProcessor::extract_client_ip(const httplib::Request& req) {
    auto x_forwarded_for = req.headers.find("X-Forwarded-For");
    if (x_forwarded_for != req.headers.end()) {
        return x_forwarded_for->second;
    }
    auto x_real_ip = req.headers.find("X-Real-IP");
    if (x_real_ip != req.headers.end()) {
        return x_real_ip->second;
    }
    return "unknown";
}

std::string SimpleHttpProcessor::generate_session_id() {
    return "session_" + std::to_string(++session_counter_);
}

}  // namespace gateway
}  // namespace im