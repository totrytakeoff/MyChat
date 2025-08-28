
/******************************************************************************
 *
 * @file       message_parser.cpp (MessageParser)
 * @brief      统一消息解析器实现 - 基于RouterManager的重构版本
 *
 * @details    实现HTTP请求和WebSocket消息到UnifiedMessage的解析转换
 *             专注于协议解析和消息格式转换，不涉及具体业务处理逻辑
 *             为上层MessageProcessor提供统一的消息格式输入
 *
 * @author     myself
 * @date       2025/08/22
 * @version    2.0.0
 *
 *****************************************************************************/


#include <nlohmann/json.hpp>
#include "message_parser.hpp"
#include "../../common/utils/log_manager.hpp"

namespace im {
namespace gateway {

using im::utils::LogManager;

// 静态成员初始化
std::atomic<uint64_t> MessageParser::session_counter_{0};

// ===== MessageParser 实现 =====

MessageParser::MessageParser(const std::string& config_file) {
    auto logger = LogManager::GetLogger("message_parser");
    logger->info("Initializing MessageParser with config: {}", config_file);

    // 1. 验证配置文件路径
    if (config_file.empty()) {
        throw std::invalid_argument("Configuration file path cannot be empty");
    }

    try {
        // 2. 初始化RouterManager
        router_manager_ = std::make_unique<RouterManager>(config_file);
        if (!router_manager_) {
            throw std::runtime_error("Failed to create RouterManager instance");
        }

        // 3. 验证配置是否加载成功
        auto router_stats = router_manager_->get_stats();
        if (router_stats.http_route_count == 0) {
            logger->warn(
                    "No routes loaded from config file, this may indicate a configuration issue");
            // 注意：不抛出异常，允许空配置（可能在运行时动态加载）
        }

        logger->info("MessageParser initialized successfully");
        logger->debug("Router statistics: {} HTTP routes, {} services",
                      router_stats.http_route_count, router_stats.service_count);

    } catch (const std::exception& e) {
        logger->error("Failed to initialize MessageParser: {}", e.what());
        throw std::runtime_error("RouterManager initialization failed: " + std::string(e.what()));
    }
}

bool MessageParser::reload_config() {
    LogManager::GetLogger("message_parser")->info("Reloading message parser configuration");

    // 使用独占锁保护配置写入
    std::unique_lock<std::shared_mutex> lock(config_mutex_);

    if (router_manager_->reload_config()) {
        LogManager::GetLogger("message_parser")->info("Configuration reloaded successfully");
        return true;
    } else {
        LogManager::GetLogger("message_parser")->error("Failed to reload configuration");
        return false;
    }
}

std::unique_ptr<UnifiedMessage> MessageParser::parse_http_request(
        const httplib::Request& req, const std::string& session_id) {
    auto logger = LogManager::GetLogger("message_parser");
    logger->debug("Parsing HTTP request: {} {}", req.method, req.path);

    // 使用共享锁保护配置读取
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    try {
        // 1. 使用RouterManager进行HTTP路由解析（只获取CMD_ID）
        auto route_result = router_manager_->parse_http_route(req.method, req.path);
        if (!route_result || !route_result->is_valid) {
            routing_failures_.fetch_add(1);
            logger->warn("HTTP routing failed for {} {}: {}", req.method, req.path,
                         route_result ? route_result->err_msg : "Unknown error");
            return nullptr;
        }

        logger->debug("HTTP routing successful: CMD_ID={}", route_result->cmd_id);

        // 2. 创建UnifiedMessage
        auto message = std::make_unique<UnifiedMessage>();

        // 3. 构造IMHeader
        im::base::IMHeader header;
        header.set_version("1.0");
        header.set_seq(0);  // HTTP请求不需要序列号
        header.set_cmd_id(route_result->cmd_id);
        header.set_token(extract_token(req));
        header.set_device_id(extract_device_id(req));
        header.set_platform(extract_platform(req));

        // 设置时间戳
        auto now = std::chrono::system_clock::now();
        auto timestamp_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                        .count();
        header.set_timestamp(static_cast<uint64_t>(timestamp_ms));


        // 4. 设置会话上下文
        UnifiedMessage::SessionContext context;
        context.protocol = UnifiedMessage::Protocol::HTTP;
        // HTTP的session_id应该基于连接信息生成，避免与WebSocket冲突
        context.session_id = session_id.empty() ? generate_http_session_id(req) : session_id;
        context.client_ip = extract_client_ip(req);
        context.receive_time = now;
        context.http_method = req.method;
        context.original_path = req.path;
        context.raw_body = req.body;

        message->set_session_context(std::move(context));

        // 5. 处理消息体和参数
        std::string message_body;

        if (req.method == "POST" && !req.body.empty()) {
            // POST请求：处理body数据
            message_body = req.body;
            message->set_json_body(req.body);
            logger->debug("Processed POST body, size: {}", req.body.size());

        } else if (req.method == "GET" && !req.params.empty()) {
            // GET请求：处理查询参数，转换为JSON格式
            nlohmann::json params_json;
            for (const auto& param : req.params) {
                params_json[param.first] = param.second;
            }
            message_body = params_json.dump();
            message->set_json_body(message_body);
            logger->debug("Processed GET params, count: {}", req.params.size());
        }

        // 提取from_uid和to_uid
        nlohmann::json get_uid = nlohmann::json::parse(message_body.c_str());
        if (get_uid.contains("from_uid")) {
            header.set_from_uid(get_uid["from_uid"].get<std::string>());
        }
        if (get_uid.contains("to_uid")) {
            header.set_to_uid(get_uid["to_uid"].get<std::string>());
        }

        // 6. 设置IMHeader
        message->set_header(std::move(header));

        // HTTP请求不需要创建额外的Protobuf消息，IMHeader已经足够
        // 上层业务可以根据需要将JSON转换为具体的Protobuf消息

        http_requests_parsed_.fetch_add(1);
        logger->debug("HTTP request processing completed successfully");
        return message;

    } catch (const std::exception& e) {
        routing_failures_.fetch_add(1);
        logger->error("Exception in HTTP request processing: {}", e.what());
        return nullptr;
    }
}

ParseResult MessageParser::parse_http_request_enhanced(
        const httplib::Request& req, const std::string& session_id) {
    auto logger = LogManager::GetLogger("message_parser");
    logger->debug("Parsing HTTP request (enhanced): {} {}", req.method, req.path);

    // 输入验证
    if (req.path.empty()) {
        return ParseResult::error_result(ParseResult::INVALID_REQUEST,
                                           "HTTP request path cannot be empty");
    }

    if (req.method.empty()) {
        return ParseResult::error_result(ParseResult::INVALID_REQUEST,
                                           "HTTP request method cannot be empty");
    }

    // 使用共享锁保护配置读取
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    try {
        // 1. 使用RouterManager进行HTTP路由解析
        auto route_result = router_manager_->parse_http_route(req.method, req.path);
        if (!route_result || !route_result->is_valid) {
            routing_failures_.fetch_add(1);
            std::string error_msg = "HTTP routing failed for " + req.method + " " + req.path;
            if (route_result) {
                error_msg += ": " + route_result->err_msg;
            }
            logger->warn(error_msg);
            return ParseResult::error_result(ParseResult::ROUTING_FAILED, error_msg);
        }

        logger->debug("HTTP routing successful: CMD_ID={}", route_result->cmd_id);

        // 2. 创建UnifiedMessage
        auto message = std::make_unique<UnifiedMessage>();

        // 3. 构造IMHeader
        im::base::IMHeader header;
        header.set_version("1.0");
        header.set_seq(0);
        header.set_cmd_id(route_result->cmd_id);
        header.set_token(extract_token(req));
        header.set_device_id(extract_device_id(req));
        header.set_platform(extract_platform(req));


        // 4. 设置会话上下文
        UnifiedMessage::SessionContext context;
        context.protocol = UnifiedMessage::Protocol::HTTP;
        context.session_id = session_id.empty() ? generate_http_session_id(req) : session_id;
        context.client_ip = extract_client_ip(req);
        context.receive_time = std::chrono::system_clock::now();
        context.http_method = req.method;
        context.original_path = req.path;
        context.raw_body = req.body;

        message->set_session_context(std::move(context));

        // 5. 处理消息体和参数
        if (req.method == "POST" && !req.body.empty()) {
            message->set_json_body(req.body);
            logger->debug("Processed POST body, size: {}", req.body.size());
        } else if (req.method == "GET" && !req.params.empty()) {
            nlohmann::json params_json;
            for (const auto& param : req.params) {
                params_json[param.first] = param.second;
            }
            message->set_json_body(params_json.dump());
            logger->debug("Processed GET params, count: {}", req.params.size());
        }

        // 提取from_uid和to_uid
        nlohmann::json get_uid = nlohmann::json::parse(message->get_json_body().c_str());
        if (get_uid.contains("from_uid")) {
            header.set_from_uid(get_uid["from_uid"].get<std::string>());
        }
        if (get_uid.contains("to_uid")) {
            header.set_to_uid(get_uid["to_uid"].get<std::string>());
        }

        // 6. 设置IMHeader
        message->set_header(std::move(header));



        http_requests_parsed_.fetch_add(1);
        logger->debug("HTTP request processing completed successfully");
        return ParseResult::success_result(std::move(message));

    } catch (const std::exception& e) {
        routing_failures_.fetch_add(1);
        std::string error_msg = "Exception in HTTP request processing: " + std::string(e.what());
        logger->error(error_msg);
        return ParseResult::error_result(ParseResult::PARSE_ERROR, error_msg);
    }
}

std::unique_ptr<UnifiedMessage> MessageParser::parse_websocket_message(
        const std::string& raw_message, const std::string& session_id) {
    auto logger = LogManager::GetLogger("message_parser");
    logger->debug("Parsing WebSocket message, size: {} bytes", raw_message.size());

    // 使用共享锁保护配置读取
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    try {
        // 1. 使用ProtobufCodec解码WebSocket消息
        im::base::IMHeader header;

        // 临时消息对象用于解析
        im::base::BaseResponse temp_message;
        if (!protobuf_codec_.decode(raw_message, header, temp_message)) {
            decode_failures_.fetch_add(1);
            logger->warn("Failed to decode WebSocket Protobuf message");
            return nullptr;
        }

        logger->debug("WebSocket message decoded successfully: CMD_ID={}", header.cmd_id());

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

        // 5. WebSocket优化：存储原始数据，避免不必要的对象创建
        // IMHeader已经包含了所有必要信息（cmd_id, token, device_id等）
        // 上层业务可以根据需要选择使用原始数据或解析为具体对象
        message->set_raw_protobuf_data(raw_message);

        // 可选：如果上层确实需要BaseResponse对象，也可以设置
        // 但大多数情况下，直接使用raw_data更高效
        if (logger->should_log(spdlog::level::debug)) {
            // 只在debug模式下创建对象用于日志
            message->set_protobuf_message(std::make_unique<im::base::BaseResponse>(temp_message));
        }

        logger->debug("WebSocket message processed efficiently with raw data preservation");

        websocket_messages_parsed_.fetch_add(1);
        logger->debug("WebSocket message processing completed successfully");
        return message;

    } catch (const std::exception& e) {
        decode_failures_.fetch_add(1);
        logger->error("Exception in WebSocket message processing: {}", e.what());
        return nullptr;
    }
}

ParseResult MessageParser::parse_websocket_message_enhanced(
        const std::string& raw_message, const std::string& session_id) {
    auto logger = LogManager::GetLogger("message_parser");
    logger->debug("Parsing WebSocket message (enhanced), size: {} bytes", raw_message.size());

    // 输入验证
    if (raw_message.empty()) {
        return ParseResult::error_result(ParseResult::INVALID_REQUEST,
                                           "WebSocket message cannot be empty");
    }

    if (raw_message.size() > 10 * 1024 * 1024) {  // 10MB limit
        return ParseResult::error_result(ParseResult::INVALID_REQUEST,
                                           "WebSocket message too large (>10MB)");
    }

    // 使用共享锁保护配置读取
    std::shared_lock<std::shared_mutex> lock(config_mutex_);

    try {
        // 1. 使用ProtobufCodec解码WebSocket消息
        im::base::IMHeader header;

        // 临时消息对象用于解析
        im::base::BaseResponse temp_message;
        if (!protobuf_codec_.decode(raw_message, header, temp_message)) {
            decode_failures_.fetch_add(1);
            std::string error_msg = "Failed to decode WebSocket Protobuf message, size: " +
                                    std::to_string(raw_message.size()) + " bytes";
            logger->warn(error_msg);
            return ParseResult::error_result(ParseResult::DECODE_FAILED, error_msg);
        }

        logger->debug("WebSocket message decoded successfully: CMD_ID={}", header.cmd_id());

        // 2. 验证解码后的数据
        if (header.cmd_id() == 0) {
            std::string error_msg = "Invalid CMD_ID (0) in WebSocket message";
            logger->warn(error_msg);
            return ParseResult::error_result(ParseResult::INVALID_REQUEST, error_msg);
        }

        // 3. 创建UnifiedMessage
        auto message = std::make_unique<UnifiedMessage>();

        // 4. 直接使用解码出的header（WebSocket的优势！）
        message->set_header(header);

        // 5. 设置会话上下文
        UnifiedMessage::SessionContext context;
        context.protocol = UnifiedMessage::Protocol::WEBSOCKET;
        context.session_id = session_id.empty() ? generate_session_id() : session_id;
        context.receive_time = std::chrono::system_clock::now();

        message->set_session_context(std::move(context));

        // 6. WebSocket优化：存储原始数据，避免不必要的对象创建
        message->set_raw_protobuf_data(raw_message);

        // 可选：如果需要BaseResponse对象，也可以设置
        if (logger->should_log(spdlog::level::debug)) {
            message->set_protobuf_message(std::make_unique<im::base::BaseResponse>(temp_message));
        }

        websocket_messages_parsed_.fetch_add(1);
        logger->debug("WebSocket message processing completed successfully");
        return ParseResult::success_result(std::move(message));

    } catch (const std::exception& e) {
        decode_failures_.fetch_add(1);
        std::string error_msg =
                "Exception in WebSocket message processing: " + std::string(e.what());
        logger->error(error_msg);
        return ParseResult::error_result(ParseResult::PARSE_ERROR, error_msg);
    }
}

// ===== 统计信息管理 =====

MessageParser::ParserStats MessageParser::get_stats() const {
    ParserStats current_stats;

    // 原子读取统计信息
    current_stats.http_requests_parsed = http_requests_parsed_.load();
    current_stats.websocket_messages_parsed = websocket_messages_parsed_.load();
    current_stats.routing_failures = routing_failures_.load();
    current_stats.decode_failures = decode_failures_.load();

    // 获取路由器统计信息
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    current_stats.router_stats = router_manager_->get_stats();

    return current_stats;
}

void MessageParser::reset_stats() {
    // 原子重置统计信息
    http_requests_parsed_.store(0);
    websocket_messages_parsed_.store(0);
    routing_failures_.store(0);
    decode_failures_.store(0);
    LogManager::GetLogger("message_parser")->info("Parser statistics reset");
}

// ===== 辅助方法实现 =====

std::string MessageParser::extract_token(const httplib::Request& req) {
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

std::string MessageParser::extract_device_id(const httplib::Request& req) {
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

std::string MessageParser::extract_platform(const httplib::Request& req) {
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

std::string MessageParser::extract_client_ip(const httplib::Request& req) {
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

std::string MessageParser::generate_session_id() {
    return "session_" + std::to_string(++session_counter_);
}

std::string MessageParser::generate_http_session_id(const httplib::Request& req) {
    // 简单的HTTP session ID生成，主要用于区分和日志追踪
    // 使用"http_"前缀避免与WebSocket session_id冲突
    return "http_" + std::to_string(++session_counter_);
}



// 移除了json_to_protobuf方法
// 上层业务可以根据需要自行实现JSON到具体Protobuf消息的转换

}  // namespace gateway
}  // namespace im