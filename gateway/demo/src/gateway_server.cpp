#include "gateway_server.hpp"
#include "common/proto/command.pb.h"
#include <algorithm>
#include <sstream>

GatewayServer::GatewayServer()
    : http_host_("0.0.0.0")
    , http_port_(8080)
    , websocket_port_(8081)
    , worker_threads_(4)
    , max_connections_(1000)
    , is_running_(false)
    , monitoring_running_(false)
    , total_requests_(0)
    , websocket_requests_(0)
    , http_requests_(0)
    , login_requests_(0)
    , successful_logins_(0) {
    
    log_mgr_ = std::make_shared<LogManager>();
    log_mgr_->Init("logs/gateway_server.log");
    
    log_mgr_->Info("GatewayServer created");
}

GatewayServer::~GatewayServer() {
    Stop();
    log_mgr_->Info("GatewayServer destroyed");
}

bool GatewayServer::Start(const std::string& config_path) {
    if (is_running_.exchange(true)) {
        log_mgr_->Warn("GatewayServer is already running");
        return false;
    }
    
    try {
        start_time_ = std::chrono::system_clock::now();
        
        // 1. 加载配置
        if (!LoadConfig(config_path)) {
            log_mgr_->Error("Failed to load configuration");
            is_running_ = false;
            return false;
        }
        
        // 2. 初始化核心模块
        if (!InitializeManagers()) {
            log_mgr_->Error("Failed to initialize managers");
            is_running_ = false;
            return false;
        }
        
        // 3. 初始化网络服务
        if (!InitializeNetwork()) {
            log_mgr_->Error("Failed to initialize network services");
            is_running_ = false;
            return false;
        }
        
        // 4. 初始化HTTP路由
        if (!InitializeHttpRoutes()) {
            log_mgr_->Error("Failed to initialize HTTP routes");
            is_running_ = false;
            return false;
        }
        
        // 5. 启动服务
        if (!ws_server_->Start(websocket_port_)) {
            log_mgr_->Error("Failed to start WebSocket server on port " + std::to_string(websocket_port_));
            is_running_ = false;
            return false;
        }
        
        // 启动HTTP服务器（在单独线程中）
        http_thread_ = std::thread([this]() {
            log_mgr_->Info("Starting HTTP server on " + http_host_ + ":" + std::to_string(http_port_));
            http_server_->listen(http_host_, http_port_);
        });
        
        // 6. 启动监控任务
        StartMonitoringTasks();
        
        log_mgr_->Info("GatewayServer started successfully");
        log_mgr_->Info("WebSocket server listening on port " + std::to_string(websocket_port_));
        log_mgr_->Info("HTTP server listening on " + http_host_ + ":" + std::to_string(http_port_));
        
        return true;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("Failed to start GatewayServer: " + std::string(e.what()));
        is_running_ = false;
        return false;
    }
}

void GatewayServer::Stop() {
    if (!is_running_.exchange(false)) {
        return; // 已经停止
    }
    
    try {
        log_mgr_->Info("Stopping GatewayServer...");
        
        // 停止监控任务
        StopMonitoringTasks();
        
        // 停止HTTP服务器
        if (http_server_) {
            http_server_->stop();
        }
        
        if (http_thread_.joinable()) {
            http_thread_.join();
        }
        
        // 停止WebSocket服务器
        if (ws_server_) {
            ws_server_->Stop();
        }
        
        // 停止核心模块
        if (route_mgr_) {
            route_mgr_->StopHealthCheck();
        }
        
        if (auth_mgr_) {
            auth_mgr_->StopCleanupTask();
        }
        
        if (conn_mgr_) {
            conn_mgr_->StopCleanupTask();
        }
        
        log_mgr_->Info("GatewayServer stopped successfully");
        
    } catch (const std::exception& e) {
        log_mgr_->Error("Error while stopping GatewayServer: " + std::string(e.what()));
    }
}

std::string GatewayServer::GetServerStats() const {
    return GenerateServerStats();
}

bool GatewayServer::LoadConfig(const std::string& config_path) {
    try {
        config_mgr_ = std::make_shared<ConfigMgr>();
        
        // 这里简化配置加载，实际项目中应该使用ConfigMgr
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            log_mgr_->Error("Cannot open config file: " + config_path);
            return false;
        }
        
        config_file >> gateway_config_;
        config_file.close();
        
        // 解析网关配置
        if (gateway_config_.contains("gateway")) {
            auto& gw_config = gateway_config_["gateway"];
            
            http_host_ = gw_config.value("http_host", "0.0.0.0");
            http_port_ = gw_config.value("http_port", 8080);
            websocket_port_ = gw_config.value("websocket_port", 8081);
            worker_threads_ = gw_config.value("worker_threads", 4);
            max_connections_ = gw_config.value("max_connections", 1000);
        }
        
        log_mgr_->Info("Configuration loaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("Failed to load configuration: " + std::string(e.what()));
        return false;
    }
}

bool GatewayServer::InitializeNetwork() {
    try {
        // 创建IO服务池
        io_service_pool_ = std::make_shared<IOServicePool>(worker_threads_);
        
        // 创建WebSocket服务器
        ws_server_ = std::make_unique<WebSocketServer>(io_service_pool_);
        
        // 设置WebSocket回调
        ws_server_->SetConnectCallback([this](std::shared_ptr<WebSocketSession> session) {
            OnWebSocketConnect(session);
        });
        
        ws_server_->SetDisconnectCallback([this](std::shared_ptr<WebSocketSession> session) {
            OnWebSocketDisconnect(session);
        });
        
        ws_server_->SetMessageCallback([this](std::shared_ptr<WebSocketSession> session, const std::string& message) {
            OnWebSocketMessage(session, message);
        });
        
        // 创建HTTP服务器
        http_server_ = std::make_unique<httplib::Server>();
        
        // 设置HTTP服务器参数
        http_server_->set_thread_pool_count(worker_threads_);
        http_server_->set_keep_alive_max_count(max_connections_);
        
        log_mgr_->Info("Network services initialized");
        return true;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("Failed to initialize network: " + std::string(e.what()));
        return false;
    }
}

bool GatewayServer::InitializeManagers() {
    try {
        // 初始化连接管理器
        conn_mgr_ = std::make_unique<ConnectionManager>();
        conn_mgr_->StartCleanupTask();
        
        // 初始化认证管理器
        auth_mgr_ = std::make_unique<AuthManager>();
        std::string secret_key = gateway_config_["auth"].value("secret_key", "default-secret-key");
        int expire_seconds = gateway_config_["auth"].value("token_expire_seconds", 86400);
        auth_mgr_->Initialize(secret_key, expire_seconds);
        
        // 初始化路由管理器
        route_mgr_ = std::make_unique<RouteManager>();
        if (gateway_config_.contains("services")) {
            route_mgr_->Initialize(gateway_config_["services"]);
        }
        
        // 初始化消息处理器
        msg_processor_ = std::make_unique<MessageProcessor>();
        
        log_mgr_->Info("Core managers initialized");
        return true;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("Failed to initialize managers: " + std::string(e.what()));
        return false;
    }
}

bool GatewayServer::InitializeHttpRoutes() {
    try {
        // 设置CORS
        http_server_->set_pre_routing_handler([this](const httplib::Request&, httplib::Response& res) {
            SetCommonHttpHeaders(res);
            return httplib::Server::HandlerResponse::Unhandled;
        });
        
        // 处理OPTIONS请求（CORS预检）
        http_server_->Options(".*", [this](const httplib::Request& req, httplib::Response& res) {
            SetCommonHttpHeaders(res);
            res.status = 200;
        });
        
        // 用户认证相关接口
        http_server_->Post("/api/v1/login", [this](const httplib::Request& req, httplib::Response& res) {
            HandleLoginRequest(req, res);
        });
        
        http_server_->Post("/api/v1/logout", [this](const httplib::Request& req, httplib::Response& res) {
            HandleLogoutRequest(req, res);
        });
        
        http_server_->Get("/api/v1/user/info", [this](const httplib::Request& req, httplib::Response& res) {
            HandleUserInfoRequest(req, res);
        });
        
        // 消息相关接口
        http_server_->Post("/api/v1/message/send", [this](const httplib::Request& req, httplib::Response& res) {
            HandleSendMessageRequest(req, res);
        });
        
        // 系统接口
        http_server_->Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
            HandleHealthCheck(req, res);
        });
        http_server_->Get("/stats", [this](const httplib::Request& req, httplib::Response& res) {
            HandleStatsRequest(req, res);
        });
        
        // 404处理
        http_server_->set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
            SetCommonHttpHeaders(res);
            if (res.status == 404) {
                json error_response;
                error_response["error_code"] = 404;
                error_response["error_message"] = "API not found";
                error_response["path"] = req.path;
                res.set_content(error_response.dump(), "application/json");
            }
        });
        
        log_mgr_->Info("HTTP routes initialized");
        return true;
        
    } catch (const std::exception& e) {
        log_mgr_->Error("Failed to initialize HTTP routes: " + std::string(e.what()));
        return false;
    }
}

void GatewayServer::OnWebSocketConnect(std::shared_ptr<WebSocketSession> session) {
    try {
        // 创建网关会话包装器
        auto gateway_session = std::make_shared<GatewayWebSocketSession>(session);
        
        // 添加到连接管理器
        conn_mgr_->AddSession(gateway_session);
        
        log_mgr_->Info("WebSocket client connected: " + gateway_session->GetSessionId());
        
    } catch (const std::exception& e) {
        log_mgr_->Error("OnWebSocketConnect error: " + std::string(e.what()));
    }
}

void GatewayServer::OnWebSocketDisconnect(std::shared_ptr<WebSocketSession> session) {
    try {
        std::string session_id = "ws_" + std::to_string(reinterpret_cast<uintptr_t>(session.get())) + "_";
        
        // 从连接管理器中移除（会自动处理用户解绑）
        conn_mgr_->RemoveSession(session_id);
        
        log_mgr_->Info("WebSocket client disconnected: " + session_id);
        
    } catch (const std::exception& e) {
        log_mgr_->Error("OnWebSocketDisconnect error: " + std::string(e.what()));
    }
}

void GatewayServer::OnWebSocketMessage(std::shared_ptr<WebSocketSession> session, 
                                      const std::string& message) {
    try {
        ++total_requests_;
        ++websocket_requests_;
        
        // 解析消息
        auto parsed_msg = msg_processor_->ParseWebSocketMessage(message);
        if (!parsed_msg.valid) {
            log_mgr_->Error("Failed to parse WebSocket message: " + parsed_msg.error_msg);
            
            // 发送错误响应
            auto error_response = MessageProcessor::CreateErrorResponse(
                im::base::INVALID_REQUEST, "Invalid message format");
            std::string response_data = msg_processor_->PackWebSocketResponse(0, 0, error_response);
            session->WriteMsg(response_data);
            return;
        }
        
        uint32_t cmd_id = parsed_msg.header.cmd_id();
        log_mgr_->Info("Received WebSocket message, cmd_id: " + std::to_string(cmd_id));
        
        // 查找网关会话
        std::string session_id = "ws_" + std::to_string(reinterpret_cast<uintptr_t>(session.get())) + "_";
        auto gateway_session = conn_mgr_->FindSessionById(session_id);
        if (!gateway_session) {
            log_mgr_->Error("Gateway session not found for WebSocket session");
            return;
        }
        
        // 特殊处理登录请求
        if (cmd_id == im::command::CMD_LOGIN) {
            ++login_requests_;
            
            try {
                json payload_json = json::parse(parsed_msg.payload);
                std::string username = payload_json.value("username", "");
                std::string password = payload_json.value("password", "");
                std::string device_id = payload_json.value("device_id", "");
                
                auto login_result = ProcessLogin(username, password, device_id, gateway_session->GetSessionId());
                
                if (login_result.error_code() == im::base::SUCCESS) {
                    ++successful_logins_;
                }
                
                std::string response_data = msg_processor_->PackWebSocketResponse(
                    parsed_msg.header.seq(), cmd_id, login_result);
                session->WriteMsg(response_data);
                
            } catch (const json::exception& e) {
                log_mgr_->Error("Login request parsing error: " + std::string(e.what()));
                auto error_response = MessageProcessor::CreateErrorResponse(
                    im::base::PARAM_ERROR, "Invalid login parameters");
                std::string response_data = msg_processor_->PackWebSocketResponse(
                    parsed_msg.header.seq(), cmd_id, error_response);
                session->WriteMsg(response_data);
            }
            return;
        }
        
        // 其他请求需要验证Token
        std::string token = parsed_msg.header.token();
        if (token.empty()) {
            auto error_response = MessageProcessor::CreateErrorResponse(
                im::base::AUTH_FAILED, "Missing authentication token");
            std::string response_data = msg_processor_->PackWebSocketResponse(
                parsed_msg.header.seq(), cmd_id, error_response);
            session->WriteMsg(response_data);
            return;
        }
        
        UserTokenInfo user_info;
        if (!auth_mgr_->VerifyTokenAndGetInfo(token, user_info)) {
            auto error_response = MessageProcessor::CreateErrorResponse(
                im::base::AUTH_FAILED, "Invalid or expired token");
            std::string response_data = msg_processor_->PackWebSocketResponse(
                parsed_msg.header.seq(), cmd_id, error_response);
            session->WriteMsg(response_data);
            return;
        }
        
        // 路由到对应服务
        route_mgr_->RouteRequest(gateway_session, parsed_msg);
        
    } catch (const std::exception& e) {
        log_mgr_->Error("OnWebSocketMessage error: " + std::string(e.what()));
        
        auto error_response = MessageProcessor::CreateErrorResponse(
            im::base::SERVER_ERROR, "Internal server error");
        std::string response_data = msg_processor_->PackWebSocketResponse(0, 0, error_response);
        session->WriteMsg(response_data);
    }
}

void GatewayServer::HandleLoginRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        ++total_requests_;
        ++http_requests_;
        ++login_requests_;
        
        SetCommonHttpHeaders(res);
        
        // 解析请求体
        json request_json = json::parse(req.body);
        
        std::string username = request_json.value("username", "");
        std::string password = request_json.value("password", "");
        std::string device_id = request_json.value("device_id", "");
        
        if (username.empty() || password.empty()) {
            json error_response;
            error_response["error_code"] = static_cast<int>(im::base::PARAM_ERROR);
            error_response["error_message"] = "Username and password are required";
            res.status = 400;
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        // 处理登录
        auto login_result = ProcessLogin(username, password, device_id);
        
        if (login_result.error_code() == im::base::SUCCESS) {
            ++successful_logins_;
            res.status = 200;
        } else {
            res.status = 401;
        }
        
        std::string response_body = msg_processor_->BaseResponseToHttpJson(login_result);
        res.set_content(response_body, "application/json");
        
    } catch (const json::exception& e) {
        log_mgr_->Error("HandleLoginRequest JSON error: " + std::string(e.what()));
        
        json error_response;
        error_response["error_code"] = static_cast<int>(im::base::PARAM_ERROR);
        error_response["error_message"] = "Invalid JSON format";
        res.status = 400;
        res.set_content(error_response.dump(), "application/json");
        
    } catch (const std::exception& e) {
        log_mgr_->Error("HandleLoginRequest error: " + std::string(e.what()));
        
        json error_response;
        error_response["error_code"] = static_cast<int>(im::base::SERVER_ERROR);
        error_response["error_message"] = "Internal server error";
        res.status = 500;
        res.set_content(error_response.dump(), "application/json");
    }
}

void GatewayServer::HandleLogoutRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        ++total_requests_;
        ++http_requests_;
        
        SetCommonHttpHeaders(res);
        
        std::string token = ExtractTokenFromRequest(req);
        if (token.empty()) {
            json error_response;
            error_response["error_code"] = static_cast<int>(im::base::AUTH_FAILED);
            error_response["error_message"] = "Missing authentication token";
            res.status = 401;
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        auto logout_result = ProcessLogout(token);
        
        std::string response_body = msg_processor_->BaseResponseToHttpJson(logout_result);
        res.status = (logout_result.error_code() == im::base::SUCCESS) ? 200 : 400;
        res.set_content(response_body, "application/json");
        
    } catch (const std::exception& e) {
        log_mgr_->Error("HandleLogoutRequest error: " + std::string(e.what()));
        
        json error_response;
        error_response["error_code"] = static_cast<int>(im::base::SERVER_ERROR);
        error_response["error_message"] = "Internal server error";
        res.status = 500;
        res.set_content(error_response.dump(), "application/json");
    }
}

void GatewayServer::HandleUserInfoRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        ++total_requests_;
        ++http_requests_;
        
        SetCommonHttpHeaders(res);
        
        std::string token = ExtractTokenFromRequest(req);
        UserTokenInfo user_info;
        
        if (!VerifyTokenAndGetUser(token, user_info)) {
            json error_response;
            error_response["error_code"] = static_cast<int>(im::base::AUTH_FAILED);
            error_response["error_message"] = "Invalid or expired token";
            res.status = 401;
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        // 构造用户信息响应
        json user_data;
        user_data["user_id"] = user_info.user_id;
        user_data["username"] = user_info.username;
        user_data["device_id"] = user_info.device_id;
        user_data["login_time"] = std::chrono::duration_cast<std::chrono::seconds>(
            user_info.create_time.time_since_epoch()).count();
        
        auto success_response = MessageProcessor::CreateSuccessResponse(user_data.dump());
        std::string response_body = msg_processor_->BaseResponseToHttpJson(success_response);
        res.set_content(response_body, "application/json");
        
    } catch (const std::exception& e) {
        log_mgr_->Error("HandleUserInfoRequest error: " + std::string(e.what()));
        
        json error_response;
        error_response["error_code"] = static_cast<int>(im::base::SERVER_ERROR);
        error_response["error_message"] = "Internal server error";
        res.status = 500;
        res.set_content(error_response.dump(), "application/json");
    }
}

void GatewayServer::HandleSendMessageRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        ++total_requests_;
        ++http_requests_;
        
        SetCommonHttpHeaders(res);
        
        std::string token = ExtractTokenFromRequest(req);
        UserTokenInfo user_info;
        
        if (!VerifyTokenAndGetUser(token, user_info)) {
            json error_response;
            error_response["error_code"] = static_cast<int>(im::base::AUTH_FAILED);
            error_response["error_message"] = "Invalid or expired token";
            res.status = 401;
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        // 解析请求并调用消息服务
        auto parsed_msg = msg_processor_->ParseHttpRequest(req.body, im::command::CMD_SEND_MESSAGE, token);
        if (!parsed_msg.valid) {
            json error_response;
            error_response["error_code"] = static_cast<int>(im::base::PARAM_ERROR);
            error_response["error_message"] = "Invalid request format";
            res.status = 400;
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        // 调用消息服务
        auto service_response = route_mgr_->CallMessageService(parsed_msg.header, parsed_msg.payload);
        
        std::string response_body = msg_processor_->BaseResponseToHttpJson(service_response);
        res.status = (service_response.error_code() == im::base::SUCCESS) ? 200 : 400;
        res.set_content(response_body, "application/json");
        
    } catch (const std::exception& e) {
        log_mgr_->Error("HandleSendMessageRequest error: " + std::string(e.what()));
        
        json error_response;
        error_response["error_code"] = static_cast<int>(im::base::SERVER_ERROR);
        error_response["error_message"] = "Internal server error";
        res.status = 500;
        res.set_content(error_response.dump(), "application/json");
    }
}

void GatewayServer::HandleHealthCheck(const httplib::Request& req, httplib::Response& res) {
    SetCommonHttpHeaders(res);
    
    json health_info;
    health_info["status"] = "ok";
    health_info["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    health_info["uptime_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - start_time_).count();
    
    // 添加基本统计信息
    health_info["connections"] = conn_mgr_->GetTotalSessions();
    health_info["online_users"] = conn_mgr_->GetOnlineUserCount();
    health_info["total_requests"] = total_requests_.load();
    
    res.set_content(health_info.dump(), "application/json");
}

void GatewayServer::HandleStatsRequest(const httplib::Request& req, httplib::Response& res) {
    SetCommonHttpHeaders(res);
    
    try {
        std::string stats = GenerateServerStats();
        res.set_content(stats, "application/json");
        
    } catch (const std::exception& e) {
        log_mgr_->Error("HandleStatsRequest error: " + std::string(e.what()));
        
        json error_response;
        error_response["error"] = "Failed to generate stats";
        res.status = 500;
        res.set_content(error_response.dump(), "application/json");
    }
}

im::base::BaseResponse GatewayServer::ProcessLogin(const std::string& username,
                                                  const std::string& password,
                                                  const std::string& device_id,
                                                  const std::string& session_id) {
    try {
        // 这里简化登录逻辑，实际项目中应该调用用户服务验证
        // 为了演示，我们假设用户名和密码都是"test"时登录成功
        
        if (username != "test" || password != "test") {
            log_mgr_->Warn("Login failed for user: " + username);
            return MessageProcessor::CreateErrorResponse(
                im::base::AUTH_FAILED, "Invalid username or password");
        }
        
        // 生成Token
        std::string user_id = "user_" + username; // 简化的用户ID
        std::string token = auth_mgr_->GenerateToken(user_id, username, device_id);
        
        if (token.empty()) {
            log_mgr_->Error("Failed to generate token for user: " + username);
            return MessageProcessor::CreateErrorResponse(
                im::base::SERVER_ERROR, "Failed to generate authentication token");
        }
        
        // 如果提供了会话ID，绑定用户到会话
        if (!session_id.empty()) {
            conn_mgr_->BindUserToSession(user_id, session_id);
        }
        
        // 构造登录成功响应
        json login_data;
        login_data["user_id"] = user_id;
        login_data["username"] = username;
        login_data["token"] = token;
        login_data["device_id"] = device_id;
        login_data["login_time"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        log_mgr_->Info("User logged in successfully: " + username);
        return MessageProcessor::CreateSuccessResponse(login_data.dump());
        
    } catch (const std::exception& e) {
        log_mgr_->Error("ProcessLogin error: " + std::string(e.what()));
        return MessageProcessor::CreateErrorResponse(
            im::base::SERVER_ERROR, "Login processing failed");
    }
}

im::base::BaseResponse GatewayServer::ProcessLogout(const std::string& token) {
    try {
        if (token.empty()) {
            return MessageProcessor::CreateErrorResponse(
                im::base::PARAM_ERROR, "Token is required");
        }
        
        // 获取用户信息
        std::string user_id;
        if (!auth_mgr_->VerifyToken(token, user_id)) {
            return MessageProcessor::CreateErrorResponse(
                im::base::AUTH_FAILED, "Invalid token");
        }
        
        // 解绑用户会话
        conn_mgr_->UnbindUser(user_id);
        
        // 撤销Token
        bool revoked = auth_mgr_->RevokeToken(token);
        
        if (!revoked) {
            log_mgr_->Warn("Failed to revoke token for user: " + user_id);
        }
        
        json logout_data;
        logout_data["user_id"] = user_id;
        logout_data["logout_time"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        log_mgr_->Info("User logged out: " + user_id);
        return MessageProcessor::CreateSuccessResponse(logout_data.dump());
        
    } catch (const std::exception& e) {
        log_mgr_->Error("ProcessLogout error: " + std::string(e.what()));
        return MessageProcessor::CreateErrorResponse(
            im::base::SERVER_ERROR, "Logout processing failed");
    }
}

bool GatewayServer::VerifyTokenAndGetUser(const std::string& token, UserTokenInfo& user_info) {
    if (token.empty()) {
        return false;
    }
    
    return auth_mgr_->VerifyTokenAndGetInfo(token, user_info);
}

std::string GatewayServer::ExtractTokenFromRequest(const httplib::Request& req) {
    // 从Authorization header中提取Bearer token
    auto auth_header = req.get_header_value("Authorization");
    if (!auth_header.empty() && auth_header.starts_with("Bearer ")) {
        return auth_header.substr(7); // 移除"Bearer "前缀
    }
    
    // 从query参数中提取token
    auto it = req.params.find("token");
    if (it != req.params.end()) {
        return it->second;
    }
    
    return "";
}

void GatewayServer::SetCommonHttpHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
    res.set_header("Access-Control-Max-Age", "86400");
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.set_header("Server", "MyChat-Gateway/1.0");
}

std::string GatewayServer::GenerateServerStats() const {
    json stats;
    
    // 基本服务信息
    stats["server"]["name"] = "MyChat-Gateway";
    stats["server"]["version"] = "1.0.0";
    stats["server"]["start_time"] = std::chrono::duration_cast<std::chrono::seconds>(
        start_time_.time_since_epoch()).count();
    stats["server"]["uptime_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - start_time_).count();
    
    // 网络配置
    stats["network"]["http_host"] = http_host_;
    stats["network"]["http_port"] = http_port_;
    stats["network"]["websocket_port"] = websocket_port_;
    stats["network"]["worker_threads"] = worker_threads_;
    
    // 请求统计
    stats["requests"]["total"] = total_requests_.load();
    stats["requests"]["websocket"] = websocket_requests_.load();
    stats["requests"]["http"] = http_requests_.load();
    stats["requests"]["login"] = login_requests_.load();
    stats["requests"]["successful_logins"] = successful_logins_.load();
    
    if (login_requests_ > 0) {
        stats["requests"]["login_success_rate"] = 
            static_cast<double>(successful_logins_) / login_requests_;
    } else {
        stats["requests"]["login_success_rate"] = 0.0;
    }
    
    // 连接统计
    if (conn_mgr_) {
        try {
            json conn_stats = json::parse(conn_mgr_->GetConnectionStats());
            stats["connections"] = conn_stats;
        } catch (...) {
            stats["connections"]["error"] = "Failed to get connection stats";
        }
    }
    
    // 认证统计
    if (auth_mgr_) {
        try {
            json auth_stats = json::parse(auth_mgr_->GetAuthStats());
            stats["authentication"] = auth_stats;
        } catch (...) {
            stats["authentication"]["error"] = "Failed to get auth stats";
        }
    }
    
    // 路由统计
    if (route_mgr_) {
        try {
            json route_stats = json::parse(route_mgr_->GetRouteStats());
            stats["routing"] = route_stats;
        } catch (...) {
            stats["routing"]["error"] = "Failed to get route stats";
        }
        
        try {
            json services_status = json::parse(route_mgr_->GetServicesStatus());
            stats["services"] = services_status;
        } catch (...) {
            stats["services"]["error"] = "Failed to get services status";
        }
    }
    
    stats["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return stats.dump(2); // 格式化输出
}

void GatewayServer::StartMonitoringTasks() {
    if (monitoring_running_.exchange(true)) {
        return; // 已经在运行
    }
    
    monitoring_thread_ = std::thread([this]() {
        log_mgr_->Info("Monitoring task started");
        
        while (monitoring_running_) {
            try {
                DoMonitoring();
            } catch (const std::exception& e) {
                log_mgr_->Error("Monitoring task error: " + std::string(e.what()));
            }
            
            // 每60秒监控一次
            for (int i = 0; i < 60 && monitoring_running_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        log_mgr_->Info("Monitoring task stopped");
    });
}

void GatewayServer::StopMonitoringTasks() {
    if (!monitoring_running_.exchange(false)) {
        return; // 已经停止
    }
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void GatewayServer::DoMonitoring() {
    // 定期输出服务状态
    static int stats_counter = 0;
    if (++stats_counter >= 5) { // 每5分钟输出一次详细统计
        stats_counter = 0;
        
        json basic_stats;
        basic_stats["uptime_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - start_time_).count();
        basic_stats["total_connections"] = conn_mgr_->GetTotalSessions();
        basic_stats["online_users"] = conn_mgr_->GetOnlineUserCount();
        basic_stats["total_requests"] = total_requests_.load();
        basic_stats["active_tokens"] = auth_mgr_->GetActiveTokenCount();
        
        log_mgr_->Info("Server stats: " + basic_stats.dump());
    }
    
    // 检查系统健康状态
    size_t total_sessions = conn_mgr_->GetTotalSessions();
    if (total_sessions > max_connections_ * 0.8) {
        log_mgr_->Warn("High connection count: " + std::to_string(total_sessions) +
                      "/" + std::to_string(max_connections_));
    }
    
    // 检查请求频率
    static size_t last_request_count = 0;
    size_t current_requests = total_requests_.load();
    size_t requests_per_minute = current_requests - last_request_count;
    last_request_count = current_requests;
    
    if (requests_per_minute > 1000) {
        log_mgr_->Warn("High request rate: " + std::to_string(requests_per_minute) + " req/min");
    }
}