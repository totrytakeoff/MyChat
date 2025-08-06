#include "message_processor.hpp"
#include "common/proto/command.pb.h"

MessageProcessor::MessageProcessor() {
    log_mgr_ = std::make_shared<LogManager>();
    log_mgr_->Init("logs/gateway.log");
}

MessageProcessor::~MessageProcessor() = default;

MessageProcessor::ParsedMessage MessageProcessor::ParseWebSocketMessage(const std::string& raw_data) {
    ParsedMessage result;
    
    try {
        // 解析JSON数据
        json msg_json = json::parse(raw_data);
        
        // 检查必要字段
        if (!msg_json.contains("header") || !msg_json.contains("payload")) {
            result.error_msg = "Missing required fields: header or payload";
            log_mgr_->Error("ParseWebSocketMessage failed: " + result.error_msg);
            return result;
        }
        
        // 解析消息头
        if (!ParseHeaderFromJson(msg_json["header"], result.header)) {
            result.error_msg = "Invalid header format";
            log_mgr_->Error("ParseWebSocketMessage failed: " + result.error_msg);
            return result;
        }
        
        // 提取payload
        if (msg_json["payload"].is_string()) {
            result.payload = msg_json["payload"].get<std::string>();
        } else {
            result.payload = msg_json["payload"].dump();
        }
        
        // 验证消息头
        if (!ValidateHeader(result.header)) {
            result.error_msg = "Header validation failed";
            log_mgr_->Error("ParseWebSocketMessage failed: " + result.error_msg);
            return result;
        }
        
        result.valid = true;
        log_mgr_->Info("Successfully parsed WebSocket message, cmd_id: " + 
                      std::to_string(result.header.cmd_id()));
        
    } catch (const json::exception& e) {
        result.error_msg = "JSON parse error: " + std::string(e.what());
        log_mgr_->Error("ParseWebSocketMessage failed: " + result.error_msg);
    } catch (const std::exception& e) {
        result.error_msg = "Unexpected error: " + std::string(e.what());
        log_mgr_->Error("ParseWebSocketMessage failed: " + result.error_msg);
    }
    
    return result;
}

MessageProcessor::ParsedMessage MessageProcessor::ParseHttpRequest(
    const std::string& req_body, 
    uint32_t cmd_id, 
    const std::string& user_token) {
    
    ParsedMessage result;
    
    try {
        // 构造消息头
        result.header.set_version("1.0");
        result.header.set_seq(1); // HTTP请求使用固定序列号
        result.header.set_cmd_id(cmd_id);
        result.header.set_timestamp(GetCurrentTimestamp());
        
        if (!user_token.empty()) {
            result.header.set_token(user_token);
        }
        
        // 解析请求体
        if (!req_body.empty()) {
            json::parse(req_body); // 验证JSON格式
            result.payload = req_body;
        } else {
            result.payload = "{}";
        }
        
        // 验证命令ID
        if (!ValidateCommand(cmd_id)) {
            result.error_msg = "Invalid command ID: " + std::to_string(cmd_id);
            log_mgr_->Error("ParseHttpRequest failed: " + result.error_msg);
            return result;
        }
        
        result.valid = true;
        log_mgr_->Info("Successfully parsed HTTP request, cmd_id: " + std::to_string(cmd_id));
        
    } catch (const json::exception& e) {
        result.error_msg = "JSON parse error: " + std::string(e.what());
        log_mgr_->Error("ParseHttpRequest failed: " + result.error_msg);
    } catch (const std::exception& e) {
        result.error_msg = "Unexpected error: " + std::string(e.what());
        log_mgr_->Error("ParseHttpRequest failed: " + result.error_msg);
    }
    
    return result;
}

std::string MessageProcessor::PackWebSocketResponse(
    uint32_t seq, 
    uint32_t cmd_id,
    const im::base::BaseResponse& response) {
    
    try {
        json response_json;
        
        // 构造响应头
        json header_json;
        header_json["version"] = "1.0";
        header_json["seq"] = seq;
        header_json["cmd_id"] = cmd_id;
        header_json["timestamp"] = GetCurrentTimestamp();
        
        // 构造响应体
        json response_body;
        response_body["error_code"] = static_cast<int>(response.error_code());
        response_body["error_message"] = response.error_message();
        
        // 处理payload
        if (!response.payload().empty()) {
            try {
                // 尝试解析为JSON
                response_body["data"] = json::parse(response.payload());
            } catch (...) {
                // 如果不是JSON，直接作为字符串
                response_body["data"] = response.payload();
            }
        }
        
        response_json["header"] = header_json;
        response_json["response"] = response_body;
        
        std::string result = response_json.dump();
        log_mgr_->Info("Packed WebSocket response for cmd_id: " + std::to_string(cmd_id));
        
        return result;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("PackWebSocketResponse failed: " + std::string(e.what()));
        
        // 返回基础错误响应
        json error_response;
        error_response["header"]["seq"] = seq;
        error_response["header"]["cmd_id"] = cmd_id;
        error_response["header"]["timestamp"] = GetCurrentTimestamp();
        error_response["response"]["error_code"] = static_cast<int>(im::base::SERVER_ERROR);
        error_response["response"]["error_message"] = "Failed to pack response";
        
        return error_response.dump();
    }
}

std::string MessageProcessor::PackPushMessage(
    const std::string& to_user,
    uint32_t cmd_id,
    const std::string& payload) {
    
    try {
        json push_json;
        
        // 构造推送头
        json header_json;
        header_json["version"] = "1.0";
        header_json["seq"] = 0; // 推送消息序列号为0
        header_json["cmd_id"] = cmd_id;
        header_json["to_uid"] = to_user;
        header_json["timestamp"] = GetCurrentTimestamp();
        
        push_json["header"] = header_json;
        
        // 处理payload
        if (!payload.empty()) {
            try {
                push_json["payload"] = json::parse(payload);
            } catch (...) {
                push_json["payload"] = payload;
            }
        } else {
            push_json["payload"] = json::object();
        }
        
        std::string result = push_json.dump();
        log_mgr_->Info("Packed push message for user: " + to_user + 
                      ", cmd_id: " + std::to_string(cmd_id));
        
        return result;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("PackPushMessage failed: " + std::string(e.what()));
        return "{}";
    }
}

std::string MessageProcessor::BaseResponseToHttpJson(const im::base::BaseResponse& response) {
    try {
        json http_json;
        http_json["error_code"] = static_cast<int>(response.error_code());
        http_json["error_message"] = response.error_message();
        
        if (!response.payload().empty()) {
            try {
                http_json["data"] = json::parse(response.payload());
            } catch (...) {
                http_json["data"] = response.payload();
            }
        }
        
        return http_json.dump();
        
    } catch (const std::exception& e) {
        log_mgr_->Error("BaseResponseToHttpJson failed: " + std::string(e.what()));
        
        json error_json;
        error_json["error_code"] = static_cast<int>(im::base::SERVER_ERROR);
        error_json["error_message"] = "Failed to convert response";
        return error_json.dump();
    }
}

im::base::BaseResponse MessageProcessor::CreateErrorResponse(
    im::base::ErrorCode error_code,
    const std::string& error_message,
    const std::string& payload) {
    
    im::base::BaseResponse response;
    response.set_error_code(error_code);
    response.set_error_message(error_message);
    if (!payload.empty()) {
        response.set_payload(payload);
    }
    return response;
}

im::base::BaseResponse MessageProcessor::CreateSuccessResponse(const std::string& payload) {
    im::base::BaseResponse response;
    response.set_error_code(im::base::SUCCESS);
    response.set_error_message("");
    response.set_payload(payload);
    return response;
}

uint64_t MessageProcessor::GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

bool MessageProcessor::ValidateHeader(const im::base::IMHeader& header) {
    // 检查必要字段
    if (header.version().empty()) {
        log_mgr_->Error("Header validation failed: missing version");
        return false;
    }
    
    if (header.cmd_id() == 0) {
        log_mgr_->Error("Header validation failed: invalid cmd_id");
        return false;
    }
    
    if (!ValidateCommand(header.cmd_id())) {
        log_mgr_->Error("Header validation failed: unknown cmd_id " + 
                       std::to_string(header.cmd_id()));
        return false;
    }
    
    return true;
}

bool MessageProcessor::ValidateCommand(uint32_t cmd_id) {
    // 验证命令ID是否在合法范围内
    if (cmd_id >= im::command::CMD_LOGIN && cmd_id <= im::command::CMD_USER_OFFLINE) {
        return true; // 用户相关命令
    }
    if (cmd_id >= im::command::CMD_SEND_MESSAGE && cmd_id <= im::command::CMD_MESSAGE_HISTORY) {
        return true; // 消息相关命令
    }
    if (cmd_id >= im::command::CMD_ADD_FRIEND && cmd_id <= im::command::CMD_SEARCH_USER) {
        return true; // 好友相关命令
    }
    if (cmd_id >= im::command::CMD_CREATE_GROUP && cmd_id <= im::command::CMD_SET_GROUP_ADMIN) {
        return true; // 群组相关命令
    }
    if (cmd_id >= im::command::CMD_PUSH_MESSAGE && cmd_id <= im::command::CMD_PUSH_OFFLINE) {
        return true; // 推送相关命令
    }
    if (cmd_id >= im::command::CMD_HEARTBEAT && cmd_id <= im::command::CMD_CLIENT_ERROR) {
        return true; // 其他命令
    }
    
    return false;
}

bool MessageProcessor::ParseHeaderFromJson(const json& header_json, im::base::IMHeader& header) {
    try {
        if (header_json.contains("version") && header_json["version"].is_string()) {
            header.set_version(header_json["version"].get<std::string>());
        }
        
        if (header_json.contains("seq") && header_json["seq"].is_number()) {
            header.set_seq(header_json["seq"].get<uint32_t>());
        }
        
        if (header_json.contains("cmd_id") && header_json["cmd_id"].is_number()) {
            header.set_cmd_id(header_json["cmd_id"].get<uint32_t>());
        }
        
        if (header_json.contains("from_uid") && header_json["from_uid"].is_string()) {
            header.set_from_uid(header_json["from_uid"].get<std::string>());
        }
        
        if (header_json.contains("to_uid") && header_json["to_uid"].is_string()) {
            header.set_to_uid(header_json["to_uid"].get<std::string>());
        }
        
        if (header_json.contains("token") && header_json["token"].is_string()) {
            header.set_token(header_json["token"].get<std::string>());
        }
        
        if (header_json.contains("device_id") && header_json["device_id"].is_string()) {
            header.set_device_id(header_json["device_id"].get<std::string>());
        }
        
        if (header_json.contains("timestamp") && header_json["timestamp"].is_number()) {
            header.set_timestamp(header_json["timestamp"].get<uint64_t>());
        } else {
            header.set_timestamp(GetCurrentTimestamp());
        }
        
        return true;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("ParseHeaderFromJson failed: " + std::string(e.what()));
        return false;
    }
}