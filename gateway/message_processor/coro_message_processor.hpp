#ifndef CORO_MESSAGE_PROCESSOR_HPP
#define CORO_MESSAGE_PROCESSOR_HPP

/******************************************************************************
 *
 * @file       coro_message_processor.hpp
 * @brief      基于协程的异步消息处理器
 *
 * @details    基于C++20协程的异步消息处理器，相比于基于std::future的版本：
 *             1. 提供更直观的异步编程体验，避免回调地狱
 *             2. 更高效的资源利用，协程开销比线程更小
 *             3. 支持复杂的异步流程控制，如并发处理、超时控制等
 *             4. 保持与原版本相同的功能：Token认证、消息路由、错误处理
 *
 * @author     myself
 * @date       2025/08/29
 * @version    2.0.0
 *
 *****************************************************************************/

#include "../auth/multi_platform_auth.hpp"
#include "../router/router_mgr.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/utils/coroutine_manager.hpp"
#include "message_parser.hpp"
#include "unified_message.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

namespace im::gateway {

// 类型别名，简化代码
using im::gateway::MessageParser;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::RouterManager;
using im::gateway::UnifiedMessage;
using im::common::Task;
using im::common::CoroutineManager;

/**
 * @brief 协程消息处理结果结构体
 * 
 * @details 与原版本ProcessorResult保持一致的接口，确保兼容性
 *          封装消息处理的结果信息，包括状态码、错误信息和响应数据
 *          支持Protobuf和JSON两种响应格式
 *  json_body: {
 *     code,
 *     body,
 *     err_msg
 *  }
 * 
 *
 */
struct CoroProcessorResult {
    int status_code;                ///< 状态码，0表示成功，其他表示错误类型
    std::string error_message;      ///< 错误信息描述
    std::string protobuf_message;   ///< Protobuf格式的响应数据 (header + body ,需配合protobufcodec进行使用)
    std::string json_body;          ///< JSON格式的响应数据 

    /**
     * @brief 默认构造函数，创建成功状态的结果
     */
    CoroProcessorResult() : status_code(0), error_message(), protobuf_message(), json_body() {}
    
    /**
     * @brief 构造错误结果
     * @param code 错误状态码
     * @param err_msg 错误信息
     */
    CoroProcessorResult(int code, std::string err_msg)
            : status_code(code)
            , error_message(std::move(err_msg))
            , protobuf_message()
            , json_body() {}
    
    /**
     * @brief 构造完整结果
     * @param code 状态码
     * @param err_msg 错误信息
     * @param pb_msg Protobuf响应数据
     * @param json JSON响应数据
     */
    CoroProcessorResult(int code, std::string err_msg, std::string pb_msg, std::string json)
            : status_code(code)
            , error_message(std::move(err_msg))
            , protobuf_message(std::move(pb_msg))
            , json_body(std::move(json)) {}
};

/**
 * @brief 协程处理函数类型定义
 * 
 * @details 处理函数返回Task<CoroProcessorResult>，支持在处理函数内部使用co_await
 *          这允许处理函数进行复杂的异步操作，如数据库查询、远程API调用等
 */
using CoroProcessorFunction = std::function<Task<CoroProcessorResult>(const UnifiedMessage&)>;

/**
 * @brief 协程处理选项配置
 * 
 * @details 提供协程处理的各种配置选项，增强灵活性和可控性
 */
struct CoroProcessingOptions {
    std::chrono::milliseconds timeout{30000};           ///< 处理超时时间，默认30秒
    bool enable_concurrent_processing{true};             ///< 是否启用并发处理
    size_t max_concurrent_tasks{100};                    ///< 最大并发任务数
    bool enable_request_logging{true};                   ///< 是否启用请求日志
    bool enable_performance_monitoring{true};           ///< 是否启用性能监控
    
    /**
     * @brief 默认构造函数，使用默认配置
     */
    CoroProcessingOptions() = default;
};

/**
 * @class CoroMessageProcessor
 * @brief 基于协程的异步消息处理器
 * 
 * @details 基于C++20协程的消息处理器，主要功能：
 *          - 协程化的消息处理流程，支持复杂异步逻辑
 *          - 保持与原版本相同的认证和路由机制
 *          - 支持超时控制和并发限制
 *          - 提供更好的异常处理和错误传播
 *          - 性能监控和请求日志记录
 * 
 * @example 典型使用方式：
 * @code
 * // 1. 创建处理器
 * CoroProcessingOptions options;
 * options.timeout = std::chrono::seconds(60);
 * auto processor = std::make_unique<CoroMessageProcessor>(router_mgr, auth_mgr, options);
 * 
 * // 2. 注册协程处理函数
 * processor->register_coro_processor(1001, [](const UnifiedMessage& msg) -> Task<CoroProcessorResult> {
 *     // 可以在这里使用co_await进行异步操作
 *     co_await 1000_ms;  // 模拟异步处理
 *     
 *     // 异步数据库查询示例
 *     auto db_result = co_await async_db_query(msg.get_user_id());
 *     
 *     co_return CoroProcessorResult(0, "", "", "{\"status\":\"success\"}");
 * });
 * 
 * // 3. 协程处理消息
 * auto result_task = processor->coro_process_message(std::move(message));
 * CoroutineManager::getInstance().schedule(std::move(result_task));
 * @endcode
 */
class CoroMessageProcessor {
public:
    /**
     * @brief 构造函数（使用现有的管理器实例）
     * @param router_mgr 路由管理器，用于服务发现和路由
     * @param auth_mgr 多平台认证管理器，用于Token验证
     * @param options 协程处理选项配置
     */
    CoroMessageProcessor(std::shared_ptr<RouterManager> router_mgr,
                         std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
                         const CoroProcessingOptions& options = CoroProcessingOptions{});

    /**
     * @brief 构造函数（从配置文件创建管理器）
     * @param router_config_file 路由配置文件路径
     * @param auth_config_file 认证配置文件路径
     * @param options 协程处理选项配置
     */
    CoroMessageProcessor(const std::string& router_config_file, 
                         const std::string& auth_config_file,
                         const CoroProcessingOptions& options = CoroProcessingOptions{});
    
    // 禁用拷贝构造和赋值
    CoroMessageProcessor(const CoroMessageProcessor&) = delete;
    CoroMessageProcessor& operator=(const CoroMessageProcessor&) = delete;

    /**
     * @brief 注册协程消息处理函数
     * 
     * @param cmd_id 命令ID，用于路由消息到对应的处理函数
     * @param processor 协程处理函数，返回Task<CoroProcessorResult>
     * 
     * @return 注册结果码:
     *         0  -> 注册成功
     *         1  -> 处理函数已存在（重复注册）
     *         -1 -> 在路由配置中未找到对应的服务
     *         -2 -> 传入的处理函数无效（nullptr）
     *         其他 -> 其他错误
     * 
     * @details 注册前会检查路由配置中是否存在对应的服务配置
     *          如果cmd_id对应的处理函数已存在，会记录警告日志但不覆盖
     */
    int register_coro_processor(uint32_t cmd_id, CoroProcessorFunction processor);

    /**
     * @brief 协程化处理消息
     * 
     * @param message 待处理的统一消息对象
     * @return Task<CoroProcessorResult> 协程任务，包含处理结果
     * 
     * @details 处理流程：
     *          1. 对于HTTP协议消息，验证Access Token
     *          2. 根据cmd_id查找对应的协程处理函数
     *          3. 执行协程处理函数，支持内部异步操作
     *          4. 应用超时控制和错误处理
     *          5. 返回处理结果
     * 
     * @note 返回的Task需要通过CoroutineManager进行调度执行
     */
    Task<CoroProcessorResult> coro_process_message(std::unique_ptr<UnifiedMessage> message);

    /**
     * @brief 带超时控制的协程消息处理
     * 
     * @param message 待处理的统一消息对象
     * @param timeout 超时时间
     * @return Task<CoroProcessorResult> 协程任务，包含处理结果
     * 
     * @details 在指定时间内完成处理，超时则返回超时错误
     */
    Task<CoroProcessorResult> coro_process_message_with_timeout(
        std::unique_ptr<UnifiedMessage> message, 
        std::chrono::milliseconds timeout);

    /**
     * @brief 批量处理多个消息（并发执行）
     * 
     * @param messages 待处理的消息列表
     * @return Task<std::vector<CoroProcessorResult>> 所有消息的处理结果
     * 
     * @details 并发处理多个消息，提高吞吐量
     *          会根据max_concurrent_tasks限制并发数量
     */
    Task<std::vector<CoroProcessorResult>> coro_process_messages_batch(
        std::vector<std::unique_ptr<UnifiedMessage>> messages);

    /**
     * @brief 获取已注册的协程处理函数数量
     * @return 当前注册的协程处理函数数量
     */
    int get_coro_callback_count() const { return coro_processor_map_.size(); }

    /**
     * @brief 获取当前活跃的协程任务数量
     * @return 正在执行的协程任务数量
     */
    size_t get_active_task_count() const { return active_task_count_; }

    /**
     * @brief 获取处理选项配置
     * @return 当前的处理选项配置
     */
    const CoroProcessingOptions& get_processing_options() const { return options_; }

    /**
     * @brief 更新处理选项配置
     * @param options 新的处理选项配置
     */
    void update_processing_options(const CoroProcessingOptions& options);

private:
    /**
     * @brief 执行Token认证验证（协程版本）
     * @param message 消息对象
     * @return Task<bool> 认证结果
     */
    Task<bool> coro_verify_authentication(const UnifiedMessage& message);

    /**
     * @brief 应用超时控制的协程包装器
     * @param task 原始协程任务
     * @param timeout 超时时间
     * @return Task<CoroProcessorResult> 带超时控制的任务
     */
    Task<CoroProcessorResult> apply_timeout_control(
        Task<CoroProcessorResult> task, 
        std::chrono::milliseconds timeout);

    /**
     * @brief 记录处理性能指标
     * @param cmd_id 命令ID
     * @param duration 处理耗时
     * @param success 是否成功
     */
    void record_performance_metrics(uint32_t cmd_id, 
                                   std::chrono::milliseconds duration, 
                                   bool success);

private:
    /// 路由管理器，用于服务发现和cmd_id到服务的映射
    std::shared_ptr<RouterManager> router_mgr_;
    
    /// 多平台认证管理器，用于Token验证和用户身份认证
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    
    /// 协程处理函数映射表，key为cmd_id，value为对应的协程处理函数
    std::unordered_map<uint32_t, CoroProcessorFunction> coro_processor_map_;
    
    /// 处理选项配置
    CoroProcessingOptions options_;
    
    /// 活跃任务计数器，用于并发控制
    std::atomic<size_t> active_task_count_{0};
    
    /// 协程管理器引用，用于协程调度
    CoroutineManager& coro_manager_;
};

}  // namespace im::gateway

/**
 * @note 协程版本使用注意事项：
 * 1. 协程处理函数可以使用co_await进行异步操作，但要注意异常处理
 * 2. 所有协程任务都需要通过CoroutineManager进行调度
 * 3. 支持超时控制，避免长时间阻塞的协程任务
 * 4. 并发处理时会自动限制最大并发数，防止资源耗尽
 * 5. 性能监控和日志记录默认开启，可通过配置关闭
 * 6. 与原版本MessageProcessor保持接口兼容性，便于迁移
 */

#endif  // CORO_MESSAGE_PROCESSOR_HPP