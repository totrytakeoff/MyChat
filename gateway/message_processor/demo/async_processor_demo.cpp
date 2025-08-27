/**
 * @file async_processor_demo.cpp
 * @brief 异步消息处理器完整演示程序
 * @details 展示三种异步模式的实际应用：Future+线程池、协程、回调链
 * 
 * @author myself
 * @date 2025/08/27
 */

#include <iostream>
#include <future>
#include <chrono>
#include <thread>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>

// 模拟你的现有模块
#include "../unified_message.hpp"
#include "../../auth/auth_mgr.hpp"
#include "../../router/router_mgr.hpp"

using namespace im::gateway;
using namespace std::chrono_literals;

// ============================================================================
// 基础结构定义
// ============================================================================

struct AuthResult {
    bool success;
    UserTokenInfo user_info;
    std::string error_msg;
    
    static AuthResult success_result(const UserTokenInfo& info) {
        return {true, info, ""};
    }
    
    static AuthResult failed_result(const std::string& error) {
        return {false, {}, error};
    }
};

struct ServiceResult {
    bool success;
    std::string data;
    int status_code;
    
    static ServiceResult success_result(const std::string& data) {
        return {true, data, 200};
    }
    
    static ServiceResult failed_result(const std::string& error, int code = 500) {
        return {false, error, code};
    }
};

struct ProcessResult {
    enum Status { SUCCESS, AUTH_FAILED, SERVICE_ERROR, TIMEOUT, UNKNOWN_COMMAND };
    
    Status status;
    std::string message;
    std::chrono::milliseconds processing_time;
    std::string trace_id;
    
    static ProcessResult success() {
        return {SUCCESS, "Processing completed successfully", 0ms, ""};
    }
    
    static ProcessResult auth_failed(const std::string& msg) {
        return {AUTH_FAILED, msg, 0ms, ""};
    }
    
    static ProcessResult service_error(const std::string& msg) {
        return {SERVICE_ERROR, msg, 0ms, ""};
    }
    
    static ProcessResult error(const std::string& msg) {
        return {SERVICE_ERROR, msg, 0ms, ""};
    }
};

// 简单的HTTP客户端模拟
class HttpClient {
public:
    struct Response {
        int status;
        std::string body;
    };
    
    Response post(const std::string& url, const std::string& data) {
        // 模拟网络延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + rand() % 100));
        
        // 模拟成功响应
        if (url.find("login") != std::string::npos) {
            return {200, R"({"success": true, "user_id": "123", "token": "abc123"})"};
        } else if (url.find("message") != std::string::npos) {
            return {200, R"({"message_id": "msg_456", "timestamp": 1640995200})"};
        } else {
            return {200, R"({"status": "ok", "data": "response_data"})"};
        }
    }
};

// ============================================================================
// 方案一：基于std::future + 线程池
// ============================================================================

class FutureBasedProcessor {
public:
    explicit FutureBasedProcessor(
        std::shared_ptr<AuthManager> auth_mgr,
        std::shared_ptr<RouterManager> router_mgr)
        : auth_mgr_(auth_mgr), router_mgr_(router_mgr) {}

    std::future<ProcessResult> process_message_async(
        std::unique_ptr<UnifiedMessage> message) {
        
        return std::async(std::launch::async, 
            [this, msg = std::move(message)]() -> ProcessResult {
            
            auto start_time = std::chrono::steady_clock::now();
            
            try {
                std::cout << "[Future] Processing cmd_id: " << msg->get_cmd_id() << std::endl;
                
                // 1. 并行启动认证和路由查找
                auto auth_future = verify_token_async(msg->get_token());
                auto route_future = find_service_async(msg->get_cmd_id());
                
                // 2. 等待认证结果
                auto auth_result = auth_future.get();
                if (!auth_result.success) {
                    std::cout << "[Future] Auth failed: " << auth_result.error_msg << std::endl;
                    return ProcessResult::auth_failed(auth_result.error_msg);
                }
                
                // 3. 等待路由结果
                auto route_result = route_future.get();
                if (!route_result) {
                    return ProcessResult::error("Service not found");
                }
                
                // 4. 异步调用微服务
                auto service_future = call_service_async(
                    route_result->endpoint, 
                    msg->get_json_body()
                );
                auto service_result = service_future.get();
                
                if (!service_result.success) {
                    return ProcessResult::service_error(service_result.data);
                }
                
                // 5. 发送响应
                send_response(*msg, service_result);
                
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);
                
                std::cout << "[Future] Completed in " << duration.count() << "ms" << std::endl;
                
                auto result = ProcessResult::success();
                result.processing_time = duration;
                return result;
                
            } catch (const std::exception& e) {
                std::cout << "[Future] Error: " << e.what() << std::endl;
                return ProcessResult::error(e.what());
            }
        });
    }

private:
    std::shared_ptr<AuthManager> auth_mgr_;
    std::shared_ptr<RouterManager> router_mgr_;
    HttpClient http_client_;
    
    std::future<AuthResult> verify_token_async(const std::string& token) {
        return std::async(std::launch::async, [this, token]() {
            std::this_thread::sleep_for(20ms); // 模拟Redis查询
            
            UserTokenInfo user_info;
            bool success = auth_mgr_->verify_token(token, user_info);
            
            if (success) {
                std::cout << "[Future] Auth success for user: " << user_info.user_id << std::endl;
                return AuthResult::success_result(user_info);
            } else {
                return AuthResult::failed_result("Invalid token");
            }
        });
    }
    
    std::future<std::unique_ptr<RouterManager::CompleteRouteResult>> find_service_async(uint32_t cmd_id) {
        return std::async(std::launch::async, [this, cmd_id]() {
            // 模拟根据cmd_id查找服务
            std::string fake_path = "/api/cmd/" + std::to_string(cmd_id);
            auto route = router_mgr_->get_complete_route("POST", fake_path);
            
            if (route) {
                std::cout << "[Future] Route found: " << route->service_name 
                         << " -> " << route->endpoint << std::endl;
            }
            
            return route;
        });
    }
    
    std::future<ServiceResult> call_service_async(
        const std::string& endpoint, 
        const std::string& data) {
        return std::async(std::launch::async, [this, endpoint, data]() {
            std::cout << "[Future] Calling service: " << endpoint << std::endl;
            
            auto response = http_client_.post(endpoint, data);
            
            if (response.status == 200) {
                return ServiceResult::success_result(response.body);
            } else {
                return ServiceResult::failed_result("Service call failed", response.status);
            }
        });
    }
    
    void send_response(const UnifiedMessage& msg, const ServiceResult& result) {
        std::cout << "[Future] Sending response to " << msg.get_session_id() 
                 << " (Protocol: " << (msg.is_http() ? "HTTP" : "WebSocket") << ")" << std::endl;
        // 实际发送逻辑...
    }
};

// ============================================================================
// 方案二：基于协程 (简化版本，用于演示概念)
// ============================================================================

// 简化的协程任务类型
template<typename T>
class SimpleTask {
public:
    SimpleTask(std::function<T()> func) : func_(func) {}
    
    T get() {
        // 简化版本：直接在后台线程执行
        auto future = std::async(std::launch::async, func_);
        return future.get();
    }
    
private:
    std::function<T()> func_;
};

class CoroutineStyleProcessor {
public:
    explicit CoroutineStyleProcessor(
        std::shared_ptr<AuthManager> auth_mgr,
        std::shared_ptr<RouterManager> router_mgr)
        : auth_mgr_(auth_mgr), router_mgr_(router_mgr) {}

    // 模拟协程风格的处理（使用lambda模拟co_await）
    std::future<ProcessResult> process_message_async(
        std::unique_ptr<UnifiedMessage> message) {
        
        return std::async(std::launch::async, 
            [this, msg = std::move(message)]() -> ProcessResult {
            
            auto start_time = std::chrono::steady_clock::now();
            
            try {
                std::cout << "[Coroutine] Processing cmd_id: " << msg->get_cmd_id() << std::endl;
                
                // 模拟 co_await verify_token_async()
                auto auth_task = verify_token_coro(msg->get_token());
                auto auth_result = auth_task.get();
                
                if (!auth_result.success) {
                    std::cout << "[Coroutine] Auth failed: " << auth_result.error_msg << std::endl;
                    return ProcessResult::auth_failed(auth_result.error_msg);
                }
                
                // 模拟 co_await find_service_async()
                auto route_task = find_service_coro(msg->get_cmd_id());
                auto route_result = route_task.get();
                
                if (!route_result) {
                    return ProcessResult::error("Service not found");
                }
                
                // 模拟 co_await call_service_async()
                auto service_task = call_service_coro(route_result->endpoint, msg->get_json_body());
                auto service_result = service_task.get();
                
                if (!service_result.success) {
                    return ProcessResult::service_error(service_result.data);
                }
                
                // 模拟 co_await send_response_async()
                auto response_task = send_response_coro(*msg, service_result);
                response_task.get();
                
                auto end_time = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);
                
                std::cout << "[Coroutine] Completed in " << duration.count() << "ms" << std::endl;
                
                auto result = ProcessResult::success();
                result.processing_time = duration;
                return result;
                
            } catch (const std::exception& e) {
                std::cout << "[Coroutine] Error: " << e.what() << std::endl;
                return ProcessResult::error(e.what());
            }
        });
    }

private:
    std::shared_ptr<AuthManager> auth_mgr_;
    std::shared_ptr<RouterManager> router_mgr_;
    HttpClient http_client_;
    
    SimpleTask<AuthResult> verify_token_coro(const std::string& token) {
        return SimpleTask<AuthResult>([this, token]() {
            std::this_thread::sleep_for(15ms); // 模拟异步操作
            
            UserTokenInfo user_info;
            bool success = auth_mgr_->verify_token(token, user_info);
            
            if (success) {
                std::cout << "[Coroutine] Auth success for user: " << user_info.user_id << std::endl;
                return AuthResult::success_result(user_info);
            } else {
                return AuthResult::failed_result("Invalid token");
            }
        });
    }
    
    SimpleTask<std::unique_ptr<RouterManager::CompleteRouteResult>> find_service_coro(uint32_t cmd_id) {
        return SimpleTask<std::unique_ptr<RouterManager::CompleteRouteResult>>([this, cmd_id]() {
            std::string fake_path = "/api/cmd/" + std::to_string(cmd_id);
            auto route = router_mgr_->get_complete_route("POST", fake_path);
            
            if (route) {
                std::cout << "[Coroutine] Route found: " << route->service_name 
                         << " -> " << route->endpoint << std::endl;
            }
            
            return route;
        });
    }
    
    SimpleTask<ServiceResult> call_service_coro(const std::string& endpoint, const std::string& data) {
        return SimpleTask<ServiceResult>([this, endpoint, data]() {
            std::cout << "[Coroutine] Calling service: " << endpoint << std::endl;
            
            auto response = http_client_.post(endpoint, data);
            
            if (response.status == 200) {
                return ServiceResult::success_result(response.body);
            } else {
                return ServiceResult::failed_result("Service call failed", response.status);
            }
        });
    }
    
    SimpleTask<void> send_response_coro(const UnifiedMessage& msg, const ServiceResult& result) {
        return SimpleTask<void>([&msg, &result]() {
            std::cout << "[Coroutine] Sending response to " << msg.get_session_id() 
                     << " (Protocol: " << (msg.is_http() ? "HTTP" : "WebSocket") << ")" << std::endl;
            // 实际发送逻辑...
        });
    }
};

// ============================================================================
// 方案三：回调链模式
// ============================================================================

class CallbackChainProcessor {
public:
    using AuthCallback = std::function<void(const AuthResult&)>;
    using ServiceCallback = std::function<void(const ServiceResult&)>;
    using ResponseCallback = std::function<void(const ProcessResult&)>;
    using CommandHandler = std::function<void(
        const UnifiedMessage&, 
        const AuthResult&,
        ServiceCallback
    )>;

    explicit CallbackChainProcessor(
        std::shared_ptr<AuthManager> auth_mgr,
        std::shared_ptr<RouterManager> router_mgr)
        : auth_mgr_(auth_mgr), router_mgr_(router_mgr) {
        register_command_handlers();
    }

    std::future<ProcessResult> process_message_async(
        std::unique_ptr<UnifiedMessage> message) {
        
        auto promise = std::make_shared<std::promise<ProcessResult>>();
        auto future = promise->get_future();
        
        auto start_time = std::make_shared<std::chrono::steady_clock::time_point>(
            std::chrono::steady_clock::now());
        
        std::cout << "[Callback] Processing cmd_id: " << message->get_cmd_id() << std::endl;
        
        // 开始异步回调链
        start_async_chain(std::move(message), promise, start_time);
        
        return future;
    }

    void register_command_handler(uint32_t cmd_id, CommandHandler handler) {
        command_handlers_[cmd_id] = std::move(handler);
    }

private:
    std::shared_ptr<AuthManager> auth_mgr_;
    std::shared_ptr<RouterManager> router_mgr_;
    std::unordered_map<uint32_t, CommandHandler> command_handlers_;
    HttpClient http_client_;
    
    void start_async_chain(
        std::unique_ptr<UnifiedMessage> message,
        std::shared_ptr<std::promise<ProcessResult>> promise,
        std::shared_ptr<std::chrono::steady_clock::time_point> start_time) {
        
        // 第一步：异步认证
        verify_token_async(message->get_token(), 
            [this, msg = std::move(message), promise, start_time]
            (const AuthResult& auth_result) mutable {
                
                if (!auth_result.success) {
                    std::cout << "[Callback] Auth failed: " << auth_result.error_msg << std::endl;
                    promise->set_value(ProcessResult::auth_failed(auth_result.error_msg));
                    return;
                }
                
                // 第二步：查找并执行命令处理器
                auto cmd_id = msg->get_cmd_id();
                auto handler_it = command_handlers_.find(cmd_id);
                
                if (handler_it == command_handlers_.end()) {
                    std::cout << "[Callback] Unknown command: " << cmd_id << std::endl;
                    promise->set_value(ProcessResult::error("Unknown command: " + std::to_string(cmd_id)));
                    return;
                }
                
                std::cout << "[Callback] Executing handler for cmd: " << cmd_id << std::endl;
                
                // 执行特定命令的处理逻辑
                handler_it->second(*msg, auth_result,
                    [this, msg = std::move(msg), promise, start_time]
                    (const ServiceResult& service_result) {
                        
                        if (!service_result.success) {
                            promise->set_value(ProcessResult::service_error(service_result.data));
                            return;
                        }
                        
                        // 第三步：发送响应
                        send_response_async(*msg, service_result,
                            [promise, start_time](const ProcessResult& final_result) {
                                auto end_time = std::chrono::steady_clock::now();
                                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    end_time - *start_time);
                                
                                auto result = final_result;
                                result.processing_time = duration;
                                
                                std::cout << "[Callback] Completed in " << duration.count() << "ms" << std::endl;
                                promise->set_value(result);
                            });
                    });
            });
    }
    
    void verify_token_async(const std::string& token, AuthCallback callback) {
        std::async(std::launch::async, [this, token, callback]() {
            std::this_thread::sleep_for(25ms); // 模拟认证延迟
            
            UserTokenInfo user_info;
            bool success = auth_mgr_->verify_token(token, user_info);
            
            AuthResult result;
            if (success) {
                std::cout << "[Callback] Auth success for user: " << user_info.user_id << std::endl;
                result = AuthResult::success_result(user_info);
            } else {
                result = AuthResult::failed_result("Invalid token");
            }
            
            callback(result);
        });
    }
    
    void send_response_async(
        const UnifiedMessage& msg, 
        const ServiceResult& result,
        ResponseCallback callback) {
        
        std::async(std::launch::async, [&msg, &result, callback]() {
            std::cout << "[Callback] Sending response to " << msg.get_session_id() 
                     << " (Protocol: " << (msg.is_http() ? "HTTP" : "WebSocket") << ")" << std::endl;
            
            // 模拟发送延迟
            std::this_thread::sleep_for(10ms);
            
            callback(ProcessResult::success());
        });
    }
    
    void register_command_handlers() {
        // 登录命令 (1001)
        register_command_handler(1001, 
            [this](const UnifiedMessage& msg, const AuthResult& auth, ServiceCallback callback) {
                std::cout << "[Callback] Handling login command" << std::endl;
                call_user_service_async("/auth/login", msg.get_json_body(), callback);
            });
        
        // 发送消息命令 (2001)  
        register_command_handler(2001,
            [this](const UnifiedMessage& msg, const AuthResult& auth, ServiceCallback callback) {
                std::cout << "[Callback] Handling send message command for user: " 
                         << auth.user_info.user_id << std::endl;
                call_chat_service_async("/message/send", msg.get_json_body(), callback);
            });
        
        // 获取好友列表 (3003)
        register_command_handler(3003,
            [this](const UnifiedMessage& msg, const AuthResult& auth, ServiceCallback callback) {
                std::cout << "[Callback] Handling friend list command for user: " 
                         << auth.user_info.user_id << std::endl;
                call_friend_service_async("/friend/list", "{\"user_id\":\"" + auth.user_info.user_id + "\"}", callback);
            });
    }
    
    void call_user_service_async(const std::string& endpoint, 
                                const std::string& data, 
                                ServiceCallback callback) {
        std::async(std::launch::async, [this, endpoint, data, callback]() {
            std::cout << "[Callback] Calling user service: " << endpoint << std::endl;
            auto response = http_client_.post("http://user-service" + endpoint, data);
            
            if (response.status == 200) {
                callback(ServiceResult::success_result(response.body));
            } else {
                callback(ServiceResult::failed_result("User service error", response.status));
            }
        });
    }
    
    void call_chat_service_async(const std::string& endpoint, 
                                const std::string& data, 
                                ServiceCallback callback) {
        std::async(std::launch::async, [this, endpoint, data, callback]() {
            std::cout << "[Callback] Calling chat service: " << endpoint << std::endl;
            auto response = http_client_.post("http://chat-service" + endpoint, data);
            
            if (response.status == 200) {
                callback(ServiceResult::success_result(response.body));
            } else {
                callback(ServiceResult::failed_result("Chat service error", response.status));
            }
        });
    }
    
    void call_friend_service_async(const std::string& endpoint, 
                                  const std::string& data, 
                                  ServiceCallback callback) {
        std::async(std::launch::async, [this, endpoint, data, callback]() {
            std::cout << "[Callback] Calling friend service: " << endpoint << std::endl;
            auto response = http_client_.post("http://friend-service" + endpoint, data);
            
            if (response.status == 200) {
                callback(ServiceResult::success_result(response.body));
            } else {
                callback(ServiceResult::failed_result("Friend service error", response.status));
            }
        });
    }
};

// ============================================================================
// 测试和演示代码
// ============================================================================

std::unique_ptr<UnifiedMessage> create_test_message(uint32_t cmd_id, const std::string& session_id) {
    auto msg = std::make_unique<UnifiedMessage>();
    
    // 设置Header
    im::base::IMHeader header;
    header.set_cmd_id(cmd_id);
    header.set_token("valid_token_123");
    header.set_device_id("device_001");
    header.set_platform("web");
    header.set_from_uid("user_123");
    header.set_timestamp(std::time(nullptr));
    
    msg->set_header(std::move(header));
    
    // 设置会话上下文
    UnifiedMessage::SessionContext context;
    context.protocol = UnifiedMessage::Protocol::HTTP;
    context.session_id = session_id;
    context.client_ip = "192.168.1.100";
    context.receive_time = std::chrono::system_clock::now();
    context.http_method = "POST";
    context.original_path = "/api/v1/test";
    
    msg->set_session_context(std::move(context));
    
    // 设置消息体
    if (cmd_id == 1001) {
        msg->set_json_body(R"({"username": "testuser", "password": "testpass"})");
    } else if (cmd_id == 2001) {
        msg->set_json_body(R"({"to_uid": "user_456", "content": "Hello World!", "type": "text"})");
    } else if (cmd_id == 3003) {
        msg->set_json_body(R"({"page": 1, "limit": 20})");
    } else {
        msg->set_json_body(R"({"test": "data"})");
    }
    
    return msg;
}

void run_performance_test() {
    std::cout << "\n=== 性能测试 ===" << std::endl;
    
    // 初始化组件
    auto auth_mgr = std::make_shared<AuthManager>("secret_key_123");
    auto router_mgr = std::make_shared<RouterManager>("../router/config/config_example.json");
    
    // 创建处理器
    auto future_processor = std::make_unique<FutureBasedProcessor>(auth_mgr, router_mgr);
    auto coroutine_processor = std::make_unique<CoroutineStyleProcessor>(auth_mgr, router_mgr);
    auto callback_processor = std::make_unique<CallbackChainProcessor>(auth_mgr, router_mgr);
    
    const int test_count = 5;
    std::vector<uint32_t> cmd_ids = {1001, 2001, 3003};
    
    // 测试Future模式
    std::cout << "\n--- Future模式测试 ---" << std::endl;
    auto future_start = std::chrono::steady_clock::now();
    
    std::vector<std::future<ProcessResult>> future_results;
    for (int i = 0; i < test_count; ++i) {
        auto cmd_id = cmd_ids[i % cmd_ids.size()];
        auto msg = create_test_message(cmd_id, "session_future_" + std::to_string(i));
        future_results.push_back(future_processor->process_message_async(std::move(msg)));
    }
    
    for (auto& future : future_results) {
        auto result = future.get();
        std::cout << "Future处理结果: " << (result.status == ProcessResult::SUCCESS ? "成功" : "失败") 
                 << ", 耗时: " << result.processing_time.count() << "ms" << std::endl;
    }
    
    auto future_end = std::chrono::steady_clock::now();
    auto future_total = std::chrono::duration_cast<std::chrono::milliseconds>(future_end - future_start);
    
    // 测试协程模式
    std::cout << "\n--- 协程模式测试 ---" << std::endl;
    auto coroutine_start = std::chrono::steady_clock::now();
    
    std::vector<std::future<ProcessResult>> coroutine_results;
    for (int i = 0; i < test_count; ++i) {
        auto cmd_id = cmd_ids[i % cmd_ids.size()];
        auto msg = create_test_message(cmd_id, "session_coroutine_" + std::to_string(i));
        coroutine_results.push_back(coroutine_processor->process_message_async(std::move(msg)));
    }
    
    for (auto& future : coroutine_results) {
        auto result = future.get();
        std::cout << "协程处理结果: " << (result.status == ProcessResult::SUCCESS ? "成功" : "失败") 
                 << ", 耗时: " << result.processing_time.count() << "ms" << std::endl;
    }
    
    auto coroutine_end = std::chrono::steady_clock::now();
    auto coroutine_total = std::chrono::duration_cast<std::chrono::milliseconds>(coroutine_end - coroutine_start);
    
    // 测试回调链模式
    std::cout << "\n--- 回调链模式测试 ---" << std::endl;
    auto callback_start = std::chrono::steady_clock::now();
    
    std::vector<std::future<ProcessResult>> callback_results;
    for (int i = 0; i < test_count; ++i) {
        auto cmd_id = cmd_ids[i % cmd_ids.size()];
        auto msg = create_test_message(cmd_id, "session_callback_" + std::to_string(i));
        callback_results.push_back(callback_processor->process_message_async(std::move(msg)));
    }
    
    for (auto& future : callback_results) {
        auto result = future.get();
        std::cout << "回调链处理结果: " << (result.status == ProcessResult::SUCCESS ? "成功" : "失败") 
                 << ", 耗时: " << result.processing_time.count() << "ms" << std::endl;
    }
    
    auto callback_end = std::chrono::steady_clock::now();
    auto callback_total = std::chrono::duration_cast<std::chrono::milliseconds>(callback_end - callback_start);
    
    // 输出性能对比
    std::cout << "\n=== 性能对比结果 ===" << std::endl;
    std::cout << "Future模式总耗时: " << future_total.count() << "ms" << std::endl;
    std::cout << "协程模式总耗时: " << coroutine_total.count() << "ms" << std::endl;
    std::cout << "回调链模式总耗时: " << callback_total.count() << "ms" << std::endl;
}

int main() {
    std::cout << "异步消息处理器演示程序" << std::endl;
    std::cout << "========================" << std::endl;
    
    try {
        run_performance_test();
        
        std::cout << "\n演示完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}