#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <future>
#include <thread>
#include <chrono>

#include "../../common/utils/coroutine_manager.hpp"
#include "../../common/utils/http_utils.hpp"
#include "gateway_server.hpp"
#include "httplib.h"

#include <nlohmann/json.hpp>


namespace im {
namespace gateway {

using im::common::CoroutineManager;
using im::common::Task;


// GatewayServer::GatewayServer(const std::string& config_file)
//         : ssl_ctx_(boost::asio::ssl::context::tlsv12_server), is_running_(false) {}

GatewayServer::GatewayServer(const std::string platform_strategy_config,
                             const std::string router_mgr, const std::string, uint16_t ws_port,
                             uint16_t http_port)
        : auth_mgr_(std::make_shared<MultiPlatformAuthManager>(platform_strategy_config))
        , router_mgr_(std::make_shared<RouterManager>(router_mgr))
        , ssl_ctx_(boost::asio::ssl::context::tlsv12_server)
        , is_running_(false)
        , psc_path_(platform_strategy_config) {
    init_server(ws_port, http_port);
}

void GatewayServer::start() {
    if (is_running_) {
        server_logger->warn("GatewayServer is already running.");
        return;
    }
    
    try {
        server_logger->info("Starting GatewayServer...");
        
        // 启动IOServicePool（必须在其他网络组件之前）
        // 注意：IOServicePool是单例，构造时已经启动了线程，这里不需要额外启动
        
        // 启动WebSocket服务器
        websocket_server_->start();
        server_logger->info("WebSocket server started");
        
        // 启动HTTP服务器（在独立线程中运行，避免阻塞）
        std::thread([this]() {
            try {
                http_server_->listen_after_bind();
            } catch (const std::exception& e) {
                server_logger->error("HTTP server error: {}", e.what());
            }
        }).detach();
        
        server_logger->info("HTTP server started");
        
        is_running_ = true;
        server_logger->info("GatewayServer started successfully");
        
    } catch (const std::exception& e) {
        server_logger->error("Failed to start GatewayServer: {}", e.what());
        is_running_ = false;
        throw;
    }
}

void GatewayServer::stop() {
    if (!is_running_) {
        server_logger->warn("GatewayServer is not running.");
        return;
    }
    is_running_ = false;
    server_logger->info("GatewayServer stop.");
    websocket_server_->stop();
    http_server_->stop();
}


std::string GatewayServer::get_server_stats() const {
    std::ostringstream ss;
    ss << "GatewayServer stats:" << std::endl;
    ss << "  Running: " << (is_running_ ? "true" : "false") << std::endl;
    ss << "online user count:" << conn_mgr_->get_online_count() << std::endl;
    ss << " processed message count:" << msg_parser_->get_stats().http_requests_parsed << std::endl;
    ss << "  processed websocket message count:"
       << msg_parser_->get_stats().websocket_messages_parsed << std::endl;
    ss << " parse.decode failed count: " << msg_parser_->get_stats().decode_failures << std::endl;
    ss << " parse.routing failed count:" << msg_parser_->get_stats().routing_failures << std::endl;
    ss << " processor.coro_callback_count: " << msg_processor_->get_coro_callback_count() << std::endl;
    ss << " processor.get_active_task_count:" << msg_processor_->get_active_task_count() << std::endl;
    return std::string(ss.str());
}

bool GatewayServer::init_server(uint16_t ws_port, uint16_t http_port, const std::string& log_path) {
    try {
        // 步骤1: 初始化日志系统
        init_logger(log_path);
        
        // 步骤2: 初始化IOServicePool (必须在WebSocketServer之前)
        init_io_service_pool();

        // 步骤3: 初始化消息解析器和处理器 (在网络服务器之前，避免回调中使用未初始化的组件)
        init_msg_parser();
        init_msg_processor();

        // 步骤4: 初始化网络服务器
        init_ws_server(ws_port);
        init_http_server(http_port);

        // 步骤5: 初始化连接管理器 (依赖websocket_server)
        init_conn_mgr();
        
        // 步骤6: 注册消息处理器
        register_message_handlers();

        server_logger->info("GatewayServer initialized successfully");
        return true;
    } catch (const std::exception& e) {
        if (server_logger) {
            server_logger->error("Failed to initialize GatewayServer: {}", e.what());
        }
        return false;
    } catch (...) {
        if (server_logger) {
            server_logger->error("Unknown exception during GatewayServer initialization");
        }
        return false;
    }
}


void GatewayServer::init_logger(const std::string& log_path) {
    // LogManager是静态类，不需要创建实例
    std::string path = log_path;
    if (!log_path.empty() && !log_path.ends_with("/")) {
        path += "/";
    }
    if (path.empty()) {
        path = "logs/";  // 默认日志路径
    }

    LogManager::SetLogToFile("gateway_server", path + "gateway_server.log");
    server_logger = LogManager::GetLogger("gateway_server");

    LogManager::SetLogToFile("io_service_pool", path + "io_service_pool.log");
    LogManager::SetLogToFile("websocket_server", path + "websocket_server.log");
    LogManager::SetLogToFile("websocket_session", path + "websocket_session.log");
    LogManager::SetLogToFile("connection_manager", path + "connection_manager.log");

    LogManager::SetLogToFile("coro_message_processor", path + "coro_message_processor.log");
    LogManager::SetLogToFile("message_parser", path + "message_parser.log");
    LogManager::SetLogToFile("message_processor", path + "message_processor.log");

    LogManager::SetLogToFile("router_manager", path + "router_manager.log");
    LogManager::SetLogToFile("service_router", path + "router_manager.log");
    LogManager::SetLogToFile("http_router", path + "router_manager.log");
    
    server_logger->info("Logger system initialized");
}

void GatewayServer::init_io_service_pool() {
    try {
        // 创建IOServicePool，使用默认线程数（CPU核心数）
        io_service_pool_ = std::make_shared<IOServicePool>();
        server_logger->info("IOServicePool initialized with default thread count");
    } catch (const std::exception& e) {
        server_logger->error("Failed to initialize IOServicePool: {}", e.what());
        throw;
    }
}

void GatewayServer::init_ws_server(uint16_t port) {
    try {
        // 构造websocket服务器消息处理函数
        std::function<void(SessionPtr, beast::flat_buffer&&)>
        message_handler([this, self = shared_from_this()](SessionPtr sessionPtr,
                                                          beast::flat_buffer&& buffer) -> void {
            auto result = this->msg_parser_->parse_websocket_message_enhanced(
                    beast::buffers_to_string(buffer.data()), sessionPtr->get_session_id());
            if (!result.success) {
                server_logger->error(
                        "parse message error in gateway.ws_server callback; error_message: {}, error_code: {}",
                        result.error_message, result.error_code);
                return; // 解析失败，直接返回
            }
            if (msg_processor_) {
                // 获取消息信息用于连接管理（在移动message之前）
                uint32_t cmd_id = result.message ? result.message->get_cmd_id() : 0;
                std::string user_id = result.message ? result.message->get_from_uid() : "";
                std::string device_id = result.message ? result.message->get_device_id() : "";
                std::string platform = result.message ? result.message->get_platform() : "";
                bool is_login_cmd = (cmd_id == 1001); // 假设1001是登录命令
                
                // 安全检查：对于需要认证的消息，检查会话是否已认证
                if (this->require_authentication_for_message(*result.message) && 
                    !this->is_session_authenticated(sessionPtr)) {
                    
                    this->server_logger->warn("Unauthorized message attempt from session {}, cmd_id: {}", 
                                            sessionPtr->get_session_id(), cmd_id);
                    
                    // 发送未认证错误响应
                    std::string error_response = HttpUtils::buildUnifiedResponse(
                        401, nullptr, "Authentication required. Please login first.");
                    sessionPtr->send(error_response);
                    return;  // 直接返回，不处理消息
                }
                
                auto coro_task =
                        this->msg_processor_->coro_process_message(std::move(result.message));

                auto&& coro_mgr = CoroutineManager::getInstance();

                coro_mgr.schedule([this, task = std::move(coro_task), sessionPtr, 
                                  is_login_cmd, user_id, device_id, platform]() mutable 
                                 -> im::common::Task<void> {
                    try {
                        auto result = co_await task;
                        // 处理结果
                        if (result.status_code != 0) {
                            // 处理错误情况
                            this->server_logger->error("处理消息时发生错误: {}", result.error_message);
                            // 发送错误响应
                            // TODO: 这里可以发送统一的错误响应格式
                        } else {
                            // 登录成功时绑定连接
                            if (is_login_cmd && !user_id.empty() && this->conn_mgr_) {
                                bool connected = this->conn_mgr_->add_connection(
                                    user_id, device_id, platform, sessionPtr);
                                if (connected) {
                                    this->server_logger->info("User {} connected on device {} ({})", 
                                                             user_id, device_id, platform);
                                } else {
                                    this->server_logger->warn("Failed to bind connection for user {} device {} ({})", 
                                                             user_id, device_id, platform);
                                }
                            }

                            // 发送响应给客户端
                            if (!result.protobuf_message.empty()) {
                                sessionPtr->send(result.protobuf_message);
                            } else if (!result.json_body.empty()) {
                                sessionPtr->send(result.json_body);
                            }
                        }
                    } catch (const std::exception& e) {
                        // 处理协程执行异常
                        this->server_logger->error(
                                "CoroMessageProcessor exception in gateway.ws_server callback: {}",
                                e.what());
                    }
                }());
            } else {
                server_logger->error("CoroMessageProcessor is not initialized.");
                init_msg_processor();
            }
        });


        websocket_server_ = std::make_unique<WebSocketServer>(io_service_pool_->GetIOService(),
                                                              ssl_ctx_, port, message_handler);
        
        // 设置连接和断开回调
        websocket_server_->set_connect_handler([this](SessionPtr session) {
            this->on_websocket_connect(session);
        });
        
        websocket_server_->set_disconnect_handler([this](SessionPtr session) {
            this->on_websocket_disconnect(session);
        });
        
        server_logger->info("WebSocket server initialized on port {}", port);

    } catch (...) {
        server_logger->error("Failed to start websocket server.");
        throw std::runtime_error("Failed to start websocket server.");
    }
}
void GatewayServer::init_http_server(uint16_t port) {
    this->http_server_ = std::make_unique<httplib::Server>();

    try {
        std::function<void(const httplib::Request& req, httplib::Response& res)> http_callback(
                [this](const httplib::Request& req, httplib::Response& res) {
                    try {
                        auto parse_result = msg_parser_->parse_http_request_enhanced(req);
                        if (!parse_result.success) {
                            // 解析错误
                            server_logger->error(
                                    "parse message error in gateway.http_server callback; "
                                    "error_message:",
                                    parse_result.error_message,
                                    " error_code:",
                                    parse_result.error_code);
                            // res.status = 500;
                            HttpUtils::buildResponse(res, 500, "", parse_result.error_message);
                            return;
                        }
                        if (msg_processor_) {
                            // HTTP需要同步等待结果，但我们可以用你的协程系统更优雅地实现
                            auto coro_task = this->msg_processor_->coro_process_message(std::move(parse_result.message));
                            
                            // 复制必要信息
                            std::string request_info = parse_result.message ? 
                                parse_result.message->format_info().str() : "unknown";
                            
                            // 创建一个同步等待协程结果的机制
                            std::atomic<bool> completed{false};
                            CoroProcessorResult final_result;
                            
                            auto&& coro_mgr = CoroutineManager::getInstance();
                            
                            // 调度协程执行
                            coro_mgr.schedule([this, &completed, &final_result, request_info,
                                             task = std::move(coro_task)]() mutable -> im::common::Task<void> {
                                try {
                                    // 优雅的co_await，就像你设计的那样！
                                    auto result = co_await task;
                                    final_result = std::move(result);
                                    completed.store(true);
                                } catch (const std::exception& e) {
                                    server_logger->error("CoroMessageProcessor exception: {}", e.what());
                                    final_result = CoroProcessorResult(-1, e.what());
                                    completed.store(true);
                                }
                            }());
                            
                            // 等待协程完成（带超时）
                            auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                            while (!completed.load() && std::chrono::steady_clock::now() < timeout) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                            
                            if (!completed.load()) {
                                server_logger->warn("HTTP request processing timeout");
                                HttpUtils::buildResponse(res, 504, "", "Request processing timeout");
                                return;
                            }
                            
                            // 处理结果
                            if (final_result.status_code != 0) {
                                server_logger->error("处理消息时发生错误: {}", final_result.error_message);
                                HttpUtils::buildResponse(res, final_result.status_code,
                                                       final_result.json_body, final_result.error_message);
                            } else {
                                if (!final_result.json_body.empty()) {
                                    const int status_code = HttpUtils::statusCodeFromJson(final_result.json_body);
                                    switch (HttpUtils::parseStatusCode(status_code)) {
                                        case HttpUtils::StatusLevel::WARNING:
                                            server_logger->warn("warning in http request: status_code:{}", status_code);
                                            break;
                                        case HttpUtils::StatusLevel::ERROR:
                                            server_logger->error("error in http request: status_code:{}, request_info:{}",
                                                               status_code, request_info);
                                            break;
                                        default: break;
                                    }
                                    HttpUtils::buildResponse(res, final_result.json_body);
                                } else {
                                    HttpUtils::buildResponse(res, 200, "", "Success");
                                }
                            }
                        }

                    } catch (...) {
                        this->server_logger->error(
                                "[GatewayServer::start] Exception caught in http_callback");
                        HttpUtils::buildResponse(res, 500, "", "Exception caught in http_callback");
                    }
                });

        http_server_->Get(".*", http_callback);
        http_server_->Post(".*", http_callback);
        http_server_->bind_to_port("0.0.0.0", port);
    } catch (...) {
        this->server_logger->error(
                "[GatewayServer::http_server] Exception caught in init http_server");
        throw std::runtime_error("Failed to start http server");
    }
}

void GatewayServer::init_conn_mgr() {
    conn_mgr_ = std::make_unique<ConnectionManager>(psc_path_, websocket_server_.get());
}

void GatewayServer::init_msg_parser() {
    msg_parser_ = std::make_unique<MessageParser>(router_mgr_);
}

void GatewayServer::init_msg_processor() {
    msg_processor_ = std::make_unique<CoroMessageProcessor>(router_mgr_, auth_mgr_);
}

bool GatewayServer::register_message_handlers(uint32_t cmd_id,message_handler handler){
    if(!msg_processor_){
        server_logger->error("CoroMessageProcessor is not initialized.");
        return false;
    }
    try{
        int ret =msg_processor_->register_coro_processor(cmd_id,handler);
        if(ret!=0){
            switch(ret){
                case 1:
                    server_logger->error("Failed to register message handler for cmd_id {}: Handler already registered",cmd_id);
                    break;
                case 2:
                    server_logger->error("Failed to register message handler for cmd_id {}: Invalid cmd_id",cmd_id);
                    break;
                default:
                    server_logger->error("Failed to register message handler for cmd_id {}: Unknown error",cmd_id);
            }
            return false;
        }
        server_logger->info("Registered message handler for cmd_id: {}",cmd_id);
        return true;
    }catch(const std::exception& e){
        server_logger->error("Failed to register message handler for cmd_id {}: {}",cmd_id,e.what());
        return false;
    }
}
void GatewayServer::register_message_handlers() {
    try {
        // register message handlers here

        // 登录处理器 (CMD 1001)
        msg_processor_->register_coro_processor(
                1001, [this](const UnifiedMessage& msg) -> Task<CoroProcessorResult> {
                    // 这里应该调用实际的用户服务进行登录验证
                    server_logger->info("Processing login for user: {}", msg.get_from_uid());
                    
                    // 模拟登录验证成功，生成Token
                    auto token_result = auth_mgr_->generate_tokens(
                        msg.get_from_uid(), 
                        msg.get_from_uid(),  // username，这里简化为user_id
                        msg.get_device_id(), 
                        msg.get_platform()
                    );
                    
                    if (token_result.success) {
                        nlohmann::json response_body;
                        response_body["access_token"] = token_result.new_access_token;
                        response_body["refresh_token"] = token_result.new_refresh_token;
                        response_body["message"] = "Login successful";
                        
                        co_return CoroProcessorResult(0, "", "", 
                            HttpUtils::buildUnifiedResponse(200, response_body));
                    } else {
                        std::string error_msg = "Login failed: " + token_result.error_message;
                        co_return CoroProcessorResult(-1, error_msg, "", 
                            HttpUtils::buildUnifiedResponse(400, nullptr, error_msg));
                    }
                });

        // Token验证处理器 (CMD 1002) - 用于已有Token的连接验证
        msg_processor_->register_coro_processor(
                1002, [this](const UnifiedMessage& msg) -> Task<CoroProcessorResult> {
                    server_logger->info("Processing token verification for session: {}", msg.get_session_id());
                    
                    // 从消息中提取Token
                    std::string token = msg.get_token();
                    if (token.empty()) {
                        co_return CoroProcessorResult(-1, "Token is required");
                    }
                    
                    // 获取会话并验证Token
                    auto session = websocket_server_->get_session(msg.get_session_id());
                    if (!session) {
                        co_return CoroProcessorResult(-1, "Session not found");
                    }
                    
                    if (verify_and_bind_connection(session, token)) {
                        nlohmann::json response_body;
                        response_body["message"] = "Token verification successful";
                        
                        co_return CoroProcessorResult(0, "", "", 
                            HttpUtils::buildUnifiedResponse(200, response_body));
                    } else {
                        std::string error_msg = "Invalid token";
                        co_return CoroProcessorResult(-1, error_msg, "", 
                            HttpUtils::buildUnifiedResponse(401, nullptr, error_msg));
                    }
                });

        // 发送消息处理器示例
        msg_processor_->register_coro_processor(
                2001, [this](const UnifiedMessage& msg) -> Task<CoroProcessorResult> {
                    server_logger->info("Processing send message from user: {} to user: {}", 
                                      msg.get_from_uid(), msg.get_to_uid());
                    
                    // 推送消息给目标用户
                    if (!msg.get_to_uid().empty()) {
                        std::string push_msg = "New message from " + msg.get_from_uid();
                        push_message_to_user(msg.get_to_uid(), push_msg);
                    }
                    
                    nlohmann::json response_body;
                    response_body["message"] = "Message sent successfully";
                    
                    co_return CoroProcessorResult(0, "", "", 
                        HttpUtils::buildUnifiedResponse(200, response_body));
                });

        server_logger->info("Message handlers registered successfully");
    } catch (const std::exception& e) {
        server_logger->error("Failed to register message handlers: {}", e.what());
        throw std::runtime_error("Failed to register message handlers.");
    }
}

// ConnectionManager集成接口实现
bool GatewayServer::push_message_to_user(const std::string& user_id, const std::string& message) {
    if (!conn_mgr_) {
        server_logger->error("ConnectionManager not initialized");
        return false;
    }
    
    try {
        auto sessions = conn_mgr_->get_user_sessions(user_id);
        bool pushed = false;
        
        for (const auto& device_session : sessions) {
            auto session = websocket_server_->get_session(device_session.session_id);
            if (session) {
                session->send(message);
                pushed = true;
            }
        }
        
        server_logger->debug("Pushed message to user {} on {} devices", user_id, sessions.size());
        return pushed;
        
    } catch (const std::exception& e) {
        server_logger->error("Failed to push message to user {}: {}", user_id, e.what());
        return false;
    }
}

bool GatewayServer::push_message_to_device(const std::string& user_id, const std::string& device_id, 
                                         const std::string& platform, const std::string& message) {
    if (!conn_mgr_) {
        server_logger->error("ConnectionManager not initialized");
        return false;
    }
    
    try {
        auto session = conn_mgr_->get_session(user_id, device_id, platform);
        if (session) {
            session->send(message);
            server_logger->debug("Pushed message to user {} device {} ({})", user_id, device_id, platform);
            return true;
        } else {
            server_logger->warn("Session not found for user {} device {} ({})", user_id, device_id, platform);
            return false;
        }
        
    } catch (const std::exception& e) {
        server_logger->error("Failed to push message to user {} device {}: {}", user_id, device_id, e.what());
        return false;
    }
}

size_t GatewayServer::get_online_count() const {
    return conn_mgr_ ? conn_mgr_->get_online_count() : 0;
}

// WebSocket连接事件处理
void GatewayServer::on_websocket_connect(SessionPtr session) {
    server_logger->info("WebSocket client connected: {} from IP: {}", 
                       session->get_session_id(), session->get_client_ip());
    
    // 检查连接时是否携带了Token
    const std::string& token = session->get_token();
    if (!token.empty()) {
        // 有Token，尝试自动验证并绑定连接
        server_logger->info("Session {} provided token, attempting automatic verification", 
                           session->get_session_id());
        
        if (verify_and_bind_connection(session, token)) {
            server_logger->info("Session {} automatically authenticated with token", 
                               session->get_session_id());
            
            // 发送连接成功消息
            nlohmann::json response_body;
            response_body["message"] = "Connected and authenticated successfully";
            response_body["session_id"] = session->get_session_id();
            
            std::string response = HttpUtils::buildUnifiedResponse(200, response_body);
            session->send(response);
        } else {
            server_logger->warn("Session {} provided invalid token, closing connection for security", 
                               session->get_session_id());
            
            // 发送认证失败消息后立即关闭连接
            nlohmann::json error_response = HttpUtils::buildUnifiedResponse(
                401, nullptr, "Token authentication failed. Connection will be closed.");
            session->send(error_response);
            
            // 延迟关闭连接，确保错误消息能发送出去
            std::thread([session]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                session->close();
            }).detach();
        }
    } else {
        // 没有Token，给予30秒时间进行登录认证
        server_logger->info("Session {} connected without token, starting 30s authentication timeout", 
                           session->get_session_id());
        
        // 发送欢迎消息，提示需要登录
        nlohmann::json response_body;
        response_body["message"] = "Connected successfully. Please login within 30 seconds.";
        response_body["session_id"] = session->get_session_id();
        response_body["timeout"] = 30;
        
        std::string response = HttpUtils::buildUnifiedResponse(200, response_body);
        session->send(response);
        
        // 启动认证超时计时器
        schedule_unauthenticated_timeout(session);
    }
}

void GatewayServer::on_websocket_disconnect(SessionPtr session) {
    server_logger->info("WebSocket client disconnected: {}", session->get_session_id());
    
    // 从连接管理器中移除连接（ConnectionManager是认证状态的唯一来源）
    if (conn_mgr_) {
        conn_mgr_->remove_connection(session);
        server_logger->debug("Removed connection from ConnectionManager: {}", session->get_session_id());
    }
}

// Token验证和连接管理
bool GatewayServer::verify_and_bind_connection(SessionPtr session, const std::string& token) {
    if (!auth_mgr_ || !conn_mgr_) {
        server_logger->error("AuthManager or ConnectionManager not initialized");
        return false;
    }
    
    try {
        // 验证Token
        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            server_logger->warn("Invalid token for session: {}", session->get_session_id());
            return false;
        }
        
        // Token验证成功，绑定连接
        bool connected = conn_mgr_->add_connection(
            user_info.user_id, 
            user_info.device_id, 
            user_info.platform, 
            session
        );
        
        if (connected) {
            server_logger->info("User {} connected via token on device {} ({})", 
                              user_info.user_id, user_info.device_id, user_info.platform);
            return true;
        } else {
            server_logger->warn("Failed to bind connection for user {} device {} ({})", 
                              user_info.user_id, user_info.device_id, user_info.platform);
            return false;
        }
        
    } catch (const std::exception& e) {
        server_logger->error("Exception in verify_and_bind_connection: {}", e.what());
        return false;
    }
}

void GatewayServer::handle_connection_with_token(SessionPtr session, const std::string& token) {
    if (verify_and_bind_connection(session, token)) {
        // Token验证成功，发送连接成功响应
        server_logger->info("Connection established with valid token for session: {}", 
                          session->get_session_id());
    } else {
        // Token验证失败，关闭连接或要求重新登录
        server_logger->warn("Connection rejected due to invalid token for session: {}", 
                          session->get_session_id());
        // 可以选择关闭连接或发送错误消息
        // session->close();
    }
}

// 安全相关方法实现
void GatewayServer::schedule_unauthenticated_timeout(SessionPtr session) {
    std::string session_id = session->get_session_id();
    
    // 使用协程管理器调度超时任务
    auto&& coro_mgr = CoroutineManager::getInstance();
    coro_mgr.schedule([this, session_id, session]() -> Task<void> {
        // 等待30秒
        co_await DelayAwaiter(std::chrono::seconds(30));
        
        // 检查会话是否已认证
        if (!is_session_authenticated(session)) {
            server_logger->warn("Session {} authentication timeout, closing connection", session_id);
            
            // 发送超时消息并关闭连接
            nlohmann::json timeout_response = HttpUtils::buildUnifiedResponse(
                408, nullptr, "Authentication timeout. Connection closed.");
            session->send(timeout_response);
            
            // 延迟关闭确保消息发送
            std::thread([session]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                session->close();
            }).detach();
        }
    }());
}

bool GatewayServer::is_session_authenticated(SessionPtr session) const {
    if (!conn_mgr_) {
        return false;
    }
    
    // 简单的认证检查：尝试通过session查找用户信息
    // 如果能找到，说明这个session已经被绑定到某个用户，即已认证
    try {
        // 通过遍历在线用户来检查session是否存在
        auto online_users = conn_mgr_->get_online_users();
        for (const auto& user_id : online_users) {
            auto user_sessions = conn_mgr_->get_user_sessions(user_id);
            for (const auto& device_session : user_sessions) {
                if (device_session.session_id == session->get_session_id()) {
                    return true;  // 找到了，说明已认证
                }
            }
        }
        return false;  // 没找到，说明未认证
    } catch (const std::exception& e) {
        server_logger->error("Error checking session authentication: {}", e.what());
        return false;
    }
}

bool GatewayServer::require_authentication_for_message(const UnifiedMessage& msg) const {
    uint32_t cmd_id = msg.get_cmd_id();
    
    // 这些命令不需要预认证（用于登录和Token验证）
    switch (cmd_id) {
        case 1001:  // 登录命令
        case 1002:  // Token验证命令
            return false;
        default:
            return true;  // 其他所有命令都需要预认证
    }
}


}  // namespace gateway
}  // namespace im
