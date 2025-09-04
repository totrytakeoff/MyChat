/******************************************************************************
 *
 * @file       coro_message_processor_example.cpp
 * @brief      协程消息处理器使用示例
 *
 * @details    展示如何使用基于协程的消息处理器，包括：
 *             - 基本的协程处理函数注册
 *             - 复杂异步操作的协程实现
 *             - 批量消息处理
 *             - 超时控制和错误处理
 *
 * @author     myself
 * @date       2025/08/29
 * @version    1.0.0
 *
 *****************************************************************************/

#include "coro_message_processor.hpp"
#include "../../common/utils/log_manager.hpp"
#include <iostream>
#include <thread>
#include <future>

namespace im::gateway::examples {

using namespace im::gateway;
using namespace im::common;

/**
 * @brief 模拟异步数据库查询的协程函数
 * @param user_id 用户ID
 * @return Task<std::string> 查询结果
 */
Task<std::string> async_db_query(const std::string& user_id) {
    // 模拟数据库查询延迟
    co_await DelayAwaiter(std::chrono::milliseconds(100));
    
    // 模拟查询结果
    co_return "user_data_for_" + user_id;
}

/**
 * @brief 模拟异步Redis缓存查询
 * @param key 缓存键
 * @return Task<std::string> 缓存值
 */
Task<std::string> async_redis_get(const std::string& key) {
    // 模拟Redis查询延迟
    co_await DelayAwaiter(std::chrono::milliseconds(50));
    
    co_return "cached_value_for_" + key;
}

/**
 * @brief 模拟异步HTTP API调用
 * @param endpoint API端点
 * @return Task<std::string> API响应
 */
Task<std::string> async_http_call(const std::string& endpoint) {
    // 模拟HTTP调用延迟
    co_await DelayAwaiter(std::chrono::milliseconds(200));
    
    co_return R"({"status": "success", "endpoint": ")" + endpoint + "\"}";
}

/**
 * @brief 用户登录处理的协程函数
 * @param message 统一消息对象
 * @return Task<CoroProcessorResult> 处理结果
 */
Task<CoroProcessorResult> handle_user_login(const UnifiedMessage& message) {
    try {
        std::string user_id = message.get_user_id();
        
        // 1. 异步查询用户信息
        auto user_data = co_await async_db_query(user_id);
        
        // 2. 异步检查缓存
        auto cache_key = "user_session_" + user_id;
        auto cached_session = co_await async_redis_get(cache_key);
        
        // 3. 构造响应数据
        std::string json_response = R"({
            "status": "success",
            "user_id": ")" + user_id + R"(",
            "user_data": ")" + user_data + R"(",
            "session_info": ")" + cached_session + R"("
        })";
        
        co_return CoroProcessorResult(0, "", "", json_response);
        
    } catch (const std::exception& e) {
        co_return CoroProcessorResult(-1, "Login failed: " + std::string(e.what()));
    }
}

/**
 * @brief 消息发送处理的协程函数
 * @param message 统一消息对象
 * @return Task<CoroProcessorResult> 处理结果
 */
Task<CoroProcessorResult> handle_send_message(const UnifiedMessage& message) {
    try {
        std::string from_user = message.get_user_id();
        // 这里应该从消息体中解析目标用户，为了演示简化处理
        std::string to_user = "target_user_123";
        
        // 1. 并发执行多个异步操作
        // 验证发送者权限
        auto sender_auth_task = async_db_query("auth_" + from_user);
        
        // 检查接收者状态
        auto receiver_status_task = async_redis_get("status_" + to_user);
        
        // 调用消息推送服务
        auto push_service_task = async_http_call("/api/push/send");
        
        // 等待所有异步操作完成
        auto sender_auth = co_await std::move(sender_auth_task);
        auto receiver_status = co_await std::move(receiver_status_task);
        auto push_result = co_await std::move(push_service_task);
        
        // 2. 处理结果
        std::string json_response = R"({
            "status": "success",
            "from_user": ")" + from_user + R"(",
            "to_user": ")" + to_user + R"(",
            "sender_auth": ")" + sender_auth + R"(",
            "receiver_status": ")" + receiver_status + R"(",
            "push_result": )" + push_result + R"(
        })";
        
        co_return CoroProcessorResult(0, "", "", json_response);
        
    } catch (const std::exception& e) {
        co_return CoroProcessorResult(-1, "Send message failed: " + std::string(e.what()));
    }
}

/**
 * @brief 复杂业务逻辑处理的协程函数（演示嵌套协程调用）
 * @param message 统一消息对象
 * @return Task<CoroProcessorResult> 处理结果
 */
Task<CoroProcessorResult> handle_complex_business(const UnifiedMessage& message) {
    try {
        std::string user_id = message.get_user_id();
        
        // 1. 多层异步调用
        auto step1_result = co_await async_db_query("step1_" + user_id);
        
        // 2. 根据第一步结果决定后续操作
        if (step1_result.find("premium") != std::string::npos) {
            // 高级用户走特殊流程
            auto premium_data = co_await async_redis_get("premium_" + user_id);
            auto premium_api = co_await async_http_call("/api/premium/process");
            
            co_return CoroProcessorResult(0, "", "", 
                R"({"type": "premium", "data": ")" + premium_data + "\"}");
        } else {
            // 普通用户流程
            auto normal_data = co_await async_db_query("normal_" + user_id);
            
            co_return CoroProcessorResult(0, "", "", 
                R"({"type": "normal", "data": ")" + normal_data + "\"}");
        }
        
    } catch (const std::exception& e) {
        co_return CoroProcessorResult(-1, "Complex business failed: " + std::string(e.what()));
    }
}

/**
 * @brief 演示协程消息处理器的基本使用
 */
void demonstrate_basic_usage() {
    std::cout << "\n=== 协程消息处理器基本使用演示 ===" << std::endl;
    
    // 1. 创建协程处理选项
    CoroProcessingOptions options;
    options.timeout = std::chrono::seconds(10);
    options.max_concurrent_tasks = 50;
    options.enable_request_logging = true;
    options.enable_performance_monitoring = true;
    
    // 2. 创建协程消息处理器（这里使用模拟的配置文件路径）
    auto processor = std::make_unique<CoroMessageProcessor>(
        "config/router.json", 
        "config/auth.json", 
        options
    );
    
    // 3. 注册协程处理函数
    processor->register_coro_processor(1001, handle_user_login);
    processor->register_coro_processor(1002, handle_send_message);
    processor->register_coro_processor(1003, handle_complex_business);
    
    std::cout << "注册了 " << processor->get_coro_callback_count() << " 个协程处理函数" << std::endl;
    
    // 4. 创建模拟消息并处理
    auto message = std::make_unique<UnifiedMessage>();
    // 这里应该设置消息的具体内容，为了演示简化处理
    
    // 5. 异步处理消息
    auto processing_task = processor->coro_process_message(std::move(message));
    
    // 6. 调度协程执行
    CoroutineManager::getInstance().schedule(std::move(processing_task));
    
    std::cout << "协程消息处理任务已调度执行" << std::endl;
}

/**
 * @brief 演示批量消息处理
 */
void demonstrate_batch_processing() {
    std::cout << "\n=== 批量消息处理演示 ===" << std::endl;
    
    CoroProcessingOptions options;
    options.enable_concurrent_processing = true;
    options.max_concurrent_tasks = 10;
    
    auto processor = std::make_unique<CoroMessageProcessor>(
        "config/router.json", 
        "config/auth.json", 
        options
    );
    
    // 注册处理函数
    processor->register_coro_processor(1001, handle_user_login);
    
    // 创建多个消息
    std::vector<std::unique_ptr<UnifiedMessage>> messages;
    for (int i = 0; i < 5; ++i) {
        auto message = std::make_unique<UnifiedMessage>();
        // 设置消息内容...
        messages.push_back(std::move(message));
    }
    
    // 批量处理
    auto batch_task = processor->coro_process_messages_batch(std::move(messages));
    
    // 调度执行
    CoroutineManager::getInstance().schedule(std::move(batch_task));
    
    std::cout << "批量消息处理任务已调度执行" << std::endl;
}

/**
 * @brief 演示超时控制
 */
void demonstrate_timeout_control() {
    std::cout << "\n=== 超时控制演示 ===" << std::endl;
    
    CoroProcessingOptions options;
    options.timeout = std::chrono::milliseconds(500); // 设置较短的超时时间
    
    auto processor = std::make_unique<CoroMessageProcessor>(
        "config/router.json", 
        "config/auth.json", 
        options
    );
    
    // 注册一个可能超时的处理函数
    processor->register_coro_processor(1004, [](const UnifiedMessage& message) -> Task<CoroProcessorResult> {
        // 模拟长时间处理
        co_await DelayAwaiter(std::chrono::seconds(2)); // 超过超时时间
        co_return CoroProcessorResult(0, "", "", R"({"result": "completed"})");
    });
    
    auto message = std::make_unique<UnifiedMessage>();
    // 设置消息内容...
    
    // 使用超时控制处理消息
    auto timeout_task = processor->coro_process_message_with_timeout(
        std::move(message), 
        std::chrono::milliseconds(1000)
    );
    
    CoroutineManager::getInstance().schedule(std::move(timeout_task));
    
    std::cout << "带超时控制的消息处理任务已调度执行" << std::endl;
}

/**
 * @brief 演示错误处理和异常传播
 */
void demonstrate_error_handling() {
    std::cout << "\n=== 错误处理演示 ===" << std::endl;
    
    auto processor = std::make_unique<CoroMessageProcessor>(
        "config/router.json", 
        "config/auth.json"
    );
    
    // 注册一个会抛出异常的处理函数
    processor->register_coro_processor(1005, [](const UnifiedMessage& message) -> Task<CoroProcessorResult> {
        co_await DelayAwaiter(std::chrono::milliseconds(100));
        
        // 模拟异常情况
        throw std::runtime_error("模拟的处理异常");
        
        co_return CoroProcessorResult(0, "", "", "{}");
    });
    
    auto message = std::make_unique<UnifiedMessage>();
    // 设置消息内容...
    
    auto error_task = processor->coro_process_message(std::move(message));
    CoroutineManager::getInstance().schedule(std::move(error_task));
    
    std::cout << "错误处理演示任务已调度执行" << std::endl;
}

} // namespace im::gateway::examples

/**
 * @brief 主函数 - 运行所有演示示例
 */
int main() {
    using namespace im::gateway::examples;
    
    std::cout << "协程消息处理器使用示例" << std::endl;
    std::cout << "========================" << std::endl;
    
    try {
        // 运行各种演示
        demonstrate_basic_usage();
        demonstrate_batch_processing();
        demonstrate_timeout_control();
        demonstrate_error_handling();
        
        // 等待协程执行完成
        std::cout << "\n等待协程任务执行完成..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        std::cout << "\n所有演示完成!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "演示过程中发生异常: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}

/**
 * @note 编译和运行说明：
 * 
 * 1. 编译命令（需要C++20支持）：
 *    g++ -std=c++20 -fcoroutines -O2 -o coro_example \
 *        coro_message_processor_example.cpp \
 *        coro_message_processor.cpp \
 *        ../../common/utils/coroutine_manager.cpp \
 *        [其他依赖的源文件] \
 *        -lpthread
 * 
 * 2. 运行前需要确保：
 *    - 配置文件路径正确
 *    - 日志系统已初始化
 *    - 依赖的管理器类可用
 * 
 * 3. 实际使用中的注意事项：
 *    - 协程处理函数应该避免长时间阻塞操作
 *    - 使用适当的超时控制防止任务挂起
 *    - 合理设置并发数量限制
 *    - 做好异常处理和错误日志记录
 */