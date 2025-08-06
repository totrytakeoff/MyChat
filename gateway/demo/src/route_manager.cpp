#include "route_manager.hpp"
#include "connection_manager.hpp"
#include "httplib.h"
#include <thread>

// 服务类型名称映射
const std::unordered_map<ServiceType, std::string> RouteManager::service_type_names_ = {
    {ServiceType::USER_SERVICE, "user_service"},
    {ServiceType::MESSAGE_SERVICE, "message_service"},
    {ServiceType::FRIEND_SERVICE, "friend_service"},
    {ServiceType::GROUP_SERVICE, "group_service"},
    {ServiceType::PUSH_SERVICE, "push_service"}
};

RouteManager::RouteManager()
    : health_check_running_(false)
    , total_requests_(0)
    , successful_requests_(0)
    , failed_requests_(0)
    , timeout_requests_(0) {
    
    log_mgr_ = std::make_shared<LogManager>();
    log_mgr_->Init("logs/route_manager.log");
    
    msg_processor_ = std::make_unique<MessageProcessor>();
    
    log_mgr_->Info("RouteManager initialized");
}

RouteManager::~RouteManager() {
    StopHealthCheck();
    log_mgr_->Info("RouteManager destroyed");
}

bool RouteManager::Initialize(const json& services_config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 解析服务配置
        for (auto& [service_name, config] : services_config.items()) {
            ServiceType service_type = ServiceType::UNKNOWN_SERVICE;
            
            if (service_name == "user_service") {
                service_type = ServiceType::USER_SERVICE;
            } else if (service_name == "message_service") {
                service_type = ServiceType::MESSAGE_SERVICE;
            } else if (service_name == "friend_service") {
                service_type = ServiceType::FRIEND_SERVICE;
            } else if (service_name == "group_service") {
                service_type = ServiceType::GROUP_SERVICE;
            } else if (service_name == "push_service") {
                service_type = ServiceType::PUSH_SERVICE;
            } else {
                log_mgr_->Warn("Unknown service type: " + service_name);
                continue;
            }
            
            std::string host = config.value("host", "127.0.0.1");
            int port = config.value("port", 8080);
            int timeout_ms = config.value("timeout_ms", 5000);
            
            auto service_config = std::make_shared<ServiceConfig>(
                service_name, host, port, timeout_ms);
            
            service_configs_[service_type] = service_config;
            
            log_mgr_->Info("Configured service: " + service_name + 
                          " at " + service_config->GetAddress());
        }
        
        if (service_configs_.empty()) {
            log_mgr_->Error("No valid service configurations found");
            return false;
        }
        
        // 启动健康检查
        StartHealthCheck();
        
        log_mgr_->Info("RouteManager initialized with " + 
                      std::to_string(service_configs_.size()) + " services");
        
        return true;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("Failed to initialize RouteManager: " + std::string(e.what()));
        return false;
    }
}

bool RouteManager::RouteRequest(std::shared_ptr<IGatewaySession> session, 
                               const MessageProcessor::ParsedMessage& parsed_msg) {
    if (!session || !parsed_msg.valid) {
        log_mgr_->Error("RouteRequest failed: invalid session or message");
        return false;
    }
    
    ++total_requests_;
    
    uint32_t cmd_id = parsed_msg.header.cmd_id();
    ServiceType service_type = GetServiceType(cmd_id);
    
    if (service_type == ServiceType::UNKNOWN_SERVICE) {
        log_mgr_->Error("Unknown command ID: " + std::to_string(cmd_id));
        SendErrorResponse(session, parsed_msg, im::base::INVALID_REQUEST,
                         "Unknown command ID");
        ++failed_requests_;
        return false;
    }
    
    // 检查服务是否可用
    if (!IsServiceAvailable(service_type)) {
        log_mgr_->Error("Service unavailable: " + ServiceTypeToString(service_type));
        SendErrorResponse(session, parsed_msg, im::base::SERVER_ERROR,
                         "Service temporarily unavailable");
        ++failed_requests_;
        return false;
    }
    
    // 异步处理请求（避免阻塞主线程）
    std::thread([this, session, parsed_msg, service_type]() {
        try {
            im::base::BaseResponse service_response;
            
            // 根据服务类型调用对应服务
            switch (service_type) {
                case ServiceType::USER_SERVICE:
                    service_response = CallUserService(parsed_msg.header, parsed_msg.payload);
                    break;
                case ServiceType::MESSAGE_SERVICE:
                    service_response = CallMessageService(parsed_msg.header, parsed_msg.payload);
                    break;
                case ServiceType::FRIEND_SERVICE:
                    service_response = CallFriendService(parsed_msg.header, parsed_msg.payload);
                    break;
                case ServiceType::GROUP_SERVICE:
                    service_response = CallGroupService(parsed_msg.header, parsed_msg.payload);
                    break;
                default:
                    service_response = MessageProcessor::CreateErrorResponse(
                        im::base::SERVER_ERROR, "Service not implemented");
                    break;
            }
            
            // 处理服务响应
            HandleServiceResponse(session, parsed_msg, service_response);
            
        } catch (const std::exception& e) {
            log_mgr_->Error("RouteRequest async error: " + std::string(e.what()));
            SendErrorResponse(session, parsed_msg, im::base::SERVER_ERROR,
                             "Internal server error");
            ++failed_requests_;
        }
    }).detach();
    
    return true;
}

im::base::BaseResponse RouteManager::CallUserService(const im::base::IMHeader& header,
                                                    const std::string& payload) {
    auto config = GetServiceConfig(ServiceType::USER_SERVICE);
    if (!config || !config->available) {
        return MessageProcessor::CreateErrorResponse(
            im::base::SERVER_ERROR, "User service unavailable");
    }
    
    std::string endpoint = GetServiceEndpoint(header.cmd_id());
    
    // 构造请求体
    json request_json;
    request_json["header"] = {
        {"version", header.version()},
        {"seq", header.seq()},
        {"cmd_id", header.cmd_id()},
        {"from_uid", header.from_uid()},
        {"to_uid", header.to_uid()},
        {"token", header.token()},
        {"device_id", header.device_id()},
        {"timestamp", header.timestamp()}
    };
    
    if (!payload.empty()) {
        try {
            request_json["payload"] = json::parse(payload);
        } catch (...) {
            request_json["payload"] = payload;
        }
    }
    
    return SendHttpRequest(*config, endpoint, request_json.dump());
}

im::base::BaseResponse RouteManager::CallMessageService(const im::base::IMHeader& header,
                                                       const std::string& payload) {
    auto config = GetServiceConfig(ServiceType::MESSAGE_SERVICE);
    if (!config || !config->available) {
        return MessageProcessor::CreateErrorResponse(
            im::base::SERVER_ERROR, "Message service unavailable");
    }
    
    std::string endpoint = GetServiceEndpoint(header.cmd_id());
    
    json request_json;
    request_json["header"] = {
        {"version", header.version()},
        {"seq", header.seq()},
        {"cmd_id", header.cmd_id()},
        {"from_uid", header.from_uid()},
        {"to_uid", header.to_uid()},
        {"token", header.token()},
        {"device_id", header.device_id()},
        {"timestamp", header.timestamp()}
    };
    
    if (!payload.empty()) {
        try {
            request_json["payload"] = json::parse(payload);
        } catch (...) {
            request_json["payload"] = payload;
        }
    }
    
    return SendHttpRequest(*config, endpoint, request_json.dump());
}

im::base::BaseResponse RouteManager::CallFriendService(const im::base::IMHeader& header,
                                                      const std::string& payload) {
    auto config = GetServiceConfig(ServiceType::FRIEND_SERVICE);
    if (!config || !config->available) {
        return MessageProcessor::CreateErrorResponse(
            im::base::SERVER_ERROR, "Friend service unavailable");
    }
    
    std::string endpoint = GetServiceEndpoint(header.cmd_id());
    
    json request_json;
    request_json["header"] = {
        {"version", header.version()},
        {"seq", header.seq()},
        {"cmd_id", header.cmd_id()},
        {"from_uid", header.from_uid()},
        {"to_uid", header.to_uid()},
        {"token", header.token()},
        {"device_id", header.device_id()},
        {"timestamp", header.timestamp()}
    };
    
    if (!payload.empty()) {
        try {
            request_json["payload"] = json::parse(payload);
        } catch (...) {
            request_json["payload"] = payload;
        }
    }
    
    return SendHttpRequest(*config, endpoint, request_json.dump());
}

im::base::BaseResponse RouteManager::CallGroupService(const im::base::IMHeader& header,
                                                     const std::string& payload) {
    auto config = GetServiceConfig(ServiceType::GROUP_SERVICE);
    if (!config || !config->available) {
        return MessageProcessor::CreateErrorResponse(
            im::base::SERVER_ERROR, "Group service unavailable");
    }
    
    std::string endpoint = GetServiceEndpoint(header.cmd_id());
    
    json request_json;
    request_json["header"] = {
        {"version", header.version()},
        {"seq", header.seq()},
        {"cmd_id", header.cmd_id()},
        {"from_uid", header.from_uid()},
        {"to_uid", header.to_uid()},
        {"token", header.token()},
        {"device_id", header.device_id()},
        {"timestamp", header.timestamp()}
    };
    
    if (!payload.empty()) {
        try {
            request_json["payload"] = json::parse(payload);
        } catch (...) {
            request_json["payload"] = payload;
        }
    }
    
    return SendHttpRequest(*config, endpoint, request_json.dump());
}

ServiceType RouteManager::GetServiceType(uint32_t cmd_id) const {
    if (cmd_id >= im::command::CMD_LOGIN && cmd_id <= im::command::CMD_USER_OFFLINE) {
        return ServiceType::USER_SERVICE;
    }
    if (cmd_id >= im::command::CMD_SEND_MESSAGE && cmd_id <= im::command::CMD_MESSAGE_HISTORY) {
        return ServiceType::MESSAGE_SERVICE;
    }
    if (cmd_id >= im::command::CMD_ADD_FRIEND && cmd_id <= im::command::CMD_SEARCH_USER) {
        return ServiceType::FRIEND_SERVICE;
    }
    if (cmd_id >= im::command::CMD_CREATE_GROUP && cmd_id <= im::command::CMD_SET_GROUP_ADMIN) {
        return ServiceType::GROUP_SERVICE;
    }
    if (cmd_id >= im::command::CMD_PUSH_MESSAGE && cmd_id <= im::command::CMD_PUSH_OFFLINE) {
        return ServiceType::PUSH_SERVICE;
    }
    
    return ServiceType::UNKNOWN_SERVICE;
}

std::shared_ptr<ServiceConfig> RouteManager::GetServiceConfig(ServiceType service_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = service_configs_.find(service_type);
    if (it != service_configs_.end()) {
        return it->second;
    }
    
    return nullptr;
}

void RouteManager::UpdateServiceStatus(ServiceType service_type, bool available) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = service_configs_.find(service_type);
    if (it != service_configs_.end()) {
        it->second->available = available;
        log_mgr_->Info("Updated service status: " + ServiceTypeToString(service_type) +
                      " -> " + (available ? "available" : "unavailable"));
    }
}

bool RouteManager::IsServiceAvailable(ServiceType service_type) const {
    auto config = GetServiceConfig(service_type);
    return config && config->available;
}

im::base::BaseResponse RouteManager::SendHttpRequest(
    const ServiceConfig& service_config,
    const std::string& endpoint,
    const std::string& request_body) const {
    
    try {
        httplib::Client client(service_config.host, service_config.port);
        client.set_connection_timeout(service_config.timeout_ms / 1000, 0);
        client.set_read_timeout(service_config.timeout_ms / 1000, 0);
        
        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"User-Agent", "MyChat-Gateway/1.0"}
        };
        
        auto result = client.Post(endpoint, headers, request_body, "application/json");
        
        if (!result) {
            log_mgr_->Error("HTTP request failed: connection error to " + 
                           service_config.GetAddress() + endpoint);
            return MessageProcessor::CreateErrorResponse(
                im::base::TIMEOUT, "Service connection failed");
        }
        
        if (result->status != 200) {
            log_mgr_->Error("HTTP request failed: status " + std::to_string(result->status) +
                           " from " + service_config.GetAddress() + endpoint);
            return MessageProcessor::CreateErrorResponse(
                im::base::SERVER_ERROR, "Service returned error: " + std::to_string(result->status));
        }
        
        // 解析响应
        try {
            json response_json = json::parse(result->body);
            
            im::base::BaseResponse response;
            
            if (response_json.contains("error_code")) {
                response.set_error_code(static_cast<im::base::ErrorCode>(
                    response_json["error_code"].get<int>()));
            } else {
                response.set_error_code(im::base::SUCCESS);
            }
            
            if (response_json.contains("error_message")) {
                response.set_error_message(response_json["error_message"].get<std::string>());
            }
            
            if (response_json.contains("data")) {
                if (response_json["data"].is_string()) {
                    response.set_payload(response_json["data"].get<std::string>());
                } else {
                    response.set_payload(response_json["data"].dump());
                }
            }
            
            return response;
            
        } catch (const json::exception& e) {
            log_mgr_->Error("Failed to parse service response: " + std::string(e.what()));
            return MessageProcessor::CreateErrorResponse(
                im::base::SERVER_ERROR, "Invalid service response format");
        }
        
    } catch (const std::exception& e) {
        log_mgr_->Error("SendHttpRequest exception: " + std::string(e.what()));
        return MessageProcessor::CreateErrorResponse(
            im::base::SERVER_ERROR, "Service request failed");
    }
}

void RouteManager::HandleServiceResponse(std::shared_ptr<IGatewaySession> session,
                                        const MessageProcessor::ParsedMessage& parsed_msg,
                                        const im::base::BaseResponse& service_response) {
    if (!session) {
        ++failed_requests_;
        return;
    }
    
    try {
        // 封装响应消息
        std::string response_data = msg_processor_->PackWebSocketResponse(
            parsed_msg.header.seq(),
            parsed_msg.header.cmd_id(),
            service_response
        );
        
        // 发送给客户端
        session->SendMessage(response_data);
        
        if (service_response.error_code() == im::base::SUCCESS) {
            ++successful_requests_;
        } else {
            ++failed_requests_;
        }
        
        log_mgr_->Info("Handled service response for cmd_id: " + 
                      std::to_string(parsed_msg.header.cmd_id()) +
                      ", error_code: " + std::to_string(service_response.error_code()));
        
    } catch (const std::exception& e) {
        log_mgr_->Error("HandleServiceResponse failed: " + std::string(e.what()));
        ++failed_requests_;
    }
}

void RouteManager::SendErrorResponse(std::shared_ptr<IGatewaySession> session,
                                    const MessageProcessor::ParsedMessage& parsed_msg,
                                    im::base::ErrorCode error_code,
                                    const std::string& error_message) {
    if (!session) {
        return;
    }
    
    try {
        auto error_response = MessageProcessor::CreateErrorResponse(error_code, error_message);
        
        std::string response_data = msg_processor_->PackWebSocketResponse(
            parsed_msg.header.seq(),
            parsed_msg.header.cmd_id(),
            error_response
        );
        
        session->SendMessage(response_data);
        
        log_mgr_->Warn("Sent error response: " + error_message + 
                      " for cmd_id: " + std::to_string(parsed_msg.header.cmd_id()));
        
    } catch (const std::exception& e) {
        log_mgr_->Error("SendErrorResponse failed: " + std::string(e.what()));
    }
}

std::string RouteManager::GetServiceEndpoint(uint32_t cmd_id) const {
    // 简化的端点映射，实际项目中可以更详细
    ServiceType service_type = GetServiceType(cmd_id);
    
    switch (service_type) {
        case ServiceType::USER_SERVICE:
            if (cmd_id == im::command::CMD_LOGIN) return "/api/v1/login";
            if (cmd_id == im::command::CMD_LOGOUT) return "/api/v1/logout";
            if (cmd_id == im::command::CMD_REGISTER) return "/api/v1/register";
            if (cmd_id == im::command::CMD_GET_USER_INFO) return "/api/v1/user/info";
            return "/api/v1/user";
            
        case ServiceType::MESSAGE_SERVICE:
            if (cmd_id == im::command::CMD_SEND_MESSAGE) return "/api/v1/message/send";
            if (cmd_id == im::command::CMD_PULL_MESSAGE) return "/api/v1/message/pull";
            if (cmd_id == im::command::CMD_MESSAGE_HISTORY) return "/api/v1/message/history";
            return "/api/v1/message";
            
        case ServiceType::FRIEND_SERVICE:
            if (cmd_id == im::command::CMD_ADD_FRIEND) return "/api/v1/friend/add";
            if (cmd_id == im::command::CMD_GET_FRIEND_LIST) return "/api/v1/friend/list";
            return "/api/v1/friend";
            
        case ServiceType::GROUP_SERVICE:
            if (cmd_id == im::command::CMD_CREATE_GROUP) return "/api/v1/group/create";
            if (cmd_id == im::command::CMD_GET_GROUP_LIST) return "/api/v1/group/list";
            return "/api/v1/group";
            
        default:
            return "/api/v1/unknown";
    }
}

std::string RouteManager::ServiceTypeToString(ServiceType service_type) const {
    auto it = service_type_names_.find(service_type);
    if (it != service_type_names_.end()) {
        return it->second;
    }
    return "unknown_service";
}

std::string RouteManager::GetRouteStats() const {
    return GenerateRouteStats();
}

std::string RouteManager::GenerateRouteStats() const {
    json stats;
    stats["total_requests"] = total_requests_.load();
    stats["successful_requests"] = successful_requests_.load();
    stats["failed_requests"] = failed_requests_.load();
    stats["timeout_requests"] = timeout_requests_.load();
    
    if (total_requests_ > 0) {
        stats["success_rate"] = static_cast<double>(successful_requests_) / total_requests_;
    } else {
        stats["success_rate"] = 0.0;
    }
    
    stats["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return stats.dump();
}

std::string RouteManager::GetServicesStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json status;
    for (const auto& [service_type, config] : service_configs_) {
        json service_info;
        service_info["name"] = config->name;
        service_info["address"] = config->GetAddress();
        service_info["available"] = config->available;
        service_info["timeout_ms"] = config->timeout_ms;
        
        status[ServiceTypeToString(service_type)] = service_info;
    }
    
    return status.dump();
}

void RouteManager::StartHealthCheck() {
    if (health_check_running_.exchange(true)) {
        return; // 已经在运行
    }
    
    health_check_thread_ = std::thread([this]() {
        log_mgr_->Info("Health check task started");
        
        while (health_check_running_) {
            try {
                DoHealthCheck();
            } catch (const std::exception& e) {
                log_mgr_->Error("Health check task error: " + std::string(e.what()));
            }
            
            // 每30秒检查一次
            for (int i = 0; i < 30 && health_check_running_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        log_mgr_->Info("Health check task stopped");
    });
}

void RouteManager::StopHealthCheck() {
    if (!health_check_running_.exchange(false)) {
        return; // 已经停止
    }
    
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
}

void RouteManager::DoHealthCheck() {
    std::vector<ServiceType> services_to_check;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [service_type, config] : service_configs_) {
            services_to_check.push_back(service_type);
        }
    }
    
    for (ServiceType service_type : services_to_check) {
        bool healthy = CheckServiceHealth(service_type);
        UpdateServiceStatus(service_type, healthy);
    }
}

bool RouteManager::CheckServiceHealth(ServiceType service_type) {
    auto config = GetServiceConfig(service_type);
    if (!config) {
        return false;
    }
    
    try {
        httplib::Client client(config->host, config->port);
        client.set_connection_timeout(2, 0); // 2秒超时
        client.set_read_timeout(2, 0);
        
        auto result = client.Get("/health");
        
        bool healthy = result && (result->status == 200 || result->status == 404);
        
        if (!healthy) {
            log_mgr_->Warn("Health check failed for " + ServiceTypeToString(service_type) +
                          " at " + config->GetAddress());
        }
        
        return healthy;
        
    } catch (const std::exception& e) {
        log_mgr_->Warn("Health check exception for " + ServiceTypeToString(service_type) +
                      ": " + e.what());
        return false;
    }
}