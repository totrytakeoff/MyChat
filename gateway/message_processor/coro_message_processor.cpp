/******************************************************************************
 *
 * @file       coro_message_processor.cpp
 * @brief      基于协程的异步消息处理器实现
 *
 * @details    实现基于C++20协程的异步消息处理逻辑，包括：
 *             - 协程化的Token认证验证
 *             - 协程消息路由和处理函数调用
 *             - 超时控制和并发管理
 *             - 异常处理和性能监控
 *
 * @author     myself
 * @date       2025/08/29
 * @version    2.0.0
 *
 *****************************************************************************/

#include "coro_message_processor.hpp"
#include "../../common/utils/log_manager.hpp"
#include <algorithm>
#include <atomic>
#include <vector>
#include <future>
#include <memory>

namespace im::gateway {

using im::base::ErrorCode;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::RouterManager;
using im::gateway::UnifiedMessage;
using im::utils::LogManager;
using im::common::DelayAwaiter;

/**
 * @brief 构造函数实现（使用现有管理器实例）
 * @details 直接使用传入的路由管理器和认证管理器实例，初始化协程管理器引用
 */
CoroMessageProcessor::CoroMessageProcessor(
    std::shared_ptr<RouterManager> router_mgr,
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
    const CoroProcessingOptions& options)
        : router_mgr_(router_mgr)
        , auth_mgr_(auth_mgr)
        , options_(options)
        , coro_manager_(CoroutineManager::getInstance()) {
    LogManager::GetLogger("coro_message_processor")
            ->info("CoroMessageProcessor initialized with existing RouterManager and AuthManager, "
                   "timeout={}ms, max_concurrent={}", 
                   options_.timeout.count(), options_.max_concurrent_tasks);
}

/**
 * @brief 构造函数实现（从配置文件创建管理器）
 * @details 根据配置文件路径创建新的管理器实例，适用于独立使用的场景
 */
CoroMessageProcessor::CoroMessageProcessor(
    const std::string& router_config_file, 
    const std::string& auth_config_file,
    const CoroProcessingOptions& options)
        : options_(options)
        , coro_manager_(CoroutineManager::getInstance()) {
    router_mgr_ = std::make_shared<RouterManager>(router_config_file);
    auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(auth_config_file);
    LogManager::GetLogger("coro_message_processor")
            ->info("CoroMessageProcessor initialized with new RouterManager and AuthManager from config files");
}

/**
 * @brief 注册协程消息处理函数的实现
 * @details 验证处理函数有效性和服务配置存在性后，将协程处理函数注册到映射表中
 */
int CoroMessageProcessor::register_coro_processor(uint32_t cmd_id, CoroProcessorFunction processor) {
    // 1. 检查processor是否有效
    if (!processor) {
        LogManager::GetLogger("coro_message_processor")
                ->error("CoroMessageProcessor::register_coro_processor: invalid processor for cmd_id: {}",
                        cmd_id);
        return -2;
    }
    
    // 2. 验证路由配置中是否存在对应的服务
    auto service = router_mgr_->find_service(cmd_id);
    if (!service || !service->is_valid) {
        LogManager::GetLogger("coro_message_processor")
                ->error("CoroMessageProcessor::register_coro_processor: service not found for cmd_id: {}",
                        cmd_id);
        return -1;
    }
    
    // 3. 检查是否已经注册过该cmd_id的处理函数
    if (coro_processor_map_.contains(cmd_id)) {
        LogManager::GetLogger("coro_message_processor")
                ->warn("CoroMessageProcessor::register_coro_processor: processor already exists for cmd_id: {}",
                       cmd_id);
        return 1;
    }
    
    // 4. 注册协程处理函数
    coro_processor_map_[cmd_id] = std::move(processor);
    LogManager::GetLogger("coro_message_processor")
            ->info("CoroMessageProcessor::register_coro_processor: coro processor registered for cmd_id: {}",
                   cmd_id);
    return 0;
}

/**
 * @brief 协程化处理消息的核心实现
 * @details 使用协程实现完整的消息处理流程，包括认证验证、处理函数查找和执行
 */
Task<CoroProcessorResult> CoroMessageProcessor::coro_process_message(
    std::unique_ptr<UnifiedMessage> message) {
    
    auto start_time = std::chrono::steady_clock::now();
    uint32_t cmd_id = 0;
    
    try {
        // 增加活跃任务计数
        ++active_task_count_;
        
        // 1. 验证消息有效性
        if (!message) {
            LogManager::GetLogger("coro_message_processor")
                    ->error("CoroMessageProcessor::coro_process_message: null message");
            --active_task_count_;
            co_return CoroProcessorResult(ErrorCode::INVALID_REQUEST, "Invalid message");
        }

        cmd_id = message->get_cmd_id();
        
        if (options_.enable_request_logging) {
            LogManager::GetLogger("coro_message_processor")
                    ->debug("CoroMessageProcessor::coro_process_message: processing cmd_id: {}, "
                           "protocol: {}, active_tasks: {}", 
                           cmd_id, 
                           static_cast<int>(message->get_protocol()),
                           active_task_count_.load());
        }

        // 2. 协程化的认证验证
        bool auth_result = co_await coro_verify_authentication(*message);
        if (!auth_result) {
            LogManager::GetLogger("coro_message_processor")
                    ->warn("CoroMessageProcessor::coro_process_message: authentication failed for cmd_id: {}", 
                           cmd_id);
            --active_task_count_;
            co_return CoroProcessorResult(ErrorCode::AUTH_FAILED, "Authentication failed");
        }

        // 3. 查找对应的协程处理函数
        auto processor_it = coro_processor_map_.find(cmd_id);
        if (processor_it == coro_processor_map_.end()) {
            LogManager::GetLogger("coro_message_processor")
                    ->error("CoroMessageProcessor::coro_process_message: no coro processor for cmd_id: {}",
                            cmd_id);
            --active_task_count_;
            co_return CoroProcessorResult(ErrorCode::NOT_FOUND, 
                                        "Unknown command: " + std::to_string(cmd_id));
        }
        
        // 4. 执行协程处理函数
        LogManager::GetLogger("coro_message_processor")
                ->debug("CoroMessageProcessor::coro_process_message: executing coro processor for cmd_id: {}", 
                       cmd_id);
        
        auto result = co_await processor_it->second(*message);
        
        // 5. 记录性能指标
        if (options_.enable_performance_monitoring) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            record_performance_metrics(cmd_id, duration, result.status_code == 0);
        }
        
        --active_task_count_;
        co_return result;

    } catch (const std::exception& e) {
        // 异常处理和日志记录
        LogManager::GetLogger("coro_message_processor")
                ->error("CoroMessageProcessor::coro_process_message: exception occurred for cmd_id: {}, error: {}", 
                       cmd_id, e.what());
        
        if (options_.enable_performance_monitoring) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            record_performance_metrics(cmd_id, duration, false);
        }
        
        --active_task_count_;
        co_return CoroProcessorResult(ErrorCode::SERVER_ERROR,
                                    std::string("Exception: ") + e.what());
    }
}

/**
 * @brief 带超时控制的协程消息处理实现
 */
Task<CoroProcessorResult> CoroMessageProcessor::coro_process_message_with_timeout(
    std::unique_ptr<UnifiedMessage> message, 
    std::chrono::milliseconds timeout) {
    
    // 使用内部的超时控制包装器
    auto processing_task = coro_process_message(std::move(message));
    co_return co_await apply_timeout_control(std::move(processing_task), timeout);
}

/**
 * @brief 批量处理多个消息的协程实现
 */
Task<std::vector<CoroProcessorResult>> CoroMessageProcessor::coro_process_messages_batch(
    std::vector<std::unique_ptr<UnifiedMessage>> messages) {
    
    std::vector<CoroProcessorResult> results;
    results.reserve(messages.size());
    
    if (!options_.enable_concurrent_processing) {
        // 顺序处理
        for (auto& message : messages) {
            auto result = co_await coro_process_message(std::move(message));
            results.push_back(std::move(result));
        }
        co_return results;
    }
    
    // 并发处理（简化版本，实际应该使用when_all等协程组合器）
    // 这里为了演示，先实现基本的批处理逻辑
    size_t batch_size = std::min(messages.size(), options_.max_concurrent_tasks);
    
    LogManager::GetLogger("coro_message_processor")
            ->info("CoroMessageProcessor::coro_process_messages_batch: processing {} messages with batch_size: {}", 
                   messages.size(), batch_size);
    
    // 分批处理 - 每批内部顺序处理，批次间并发
    for (size_t i = 0; i < messages.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, messages.size());
        
        // 收集当前批次的任务
        std::vector<Task<CoroProcessorResult>> batch_tasks;
        for (size_t j = i; j < end; ++j) {
            // 注意：这里不能直接move，因为我们需要等待所有任务
            // 在实际实现中，应该使用协程组合器来真正并发执行
            auto result = co_await coro_process_message(std::move(messages[j]));
            results.push_back(std::move(result));
        }
        
        // 如果不是最后一批，稍作延迟避免资源争用
        if (end < messages.size()) {
            co_await DelayAwaiter(std::chrono::milliseconds(10));
        }
    }
    
    co_return results;
}

/**
 * @brief 更新处理选项配置
 */
void CoroMessageProcessor::update_processing_options(const CoroProcessingOptions& options) {
    options_ = options;
    LogManager::GetLogger("coro_message_processor")
            ->info("CoroMessageProcessor::update_processing_options: updated options, "
                   "timeout={}ms, max_concurrent={}", 
                   options_.timeout.count(), options_.max_concurrent_tasks);
}

/**
 * @brief 协程化的Token认证验证实现
 */
Task<bool> CoroMessageProcessor::coro_verify_authentication(const UnifiedMessage& message) {
    // 新架构：HTTP和WebSocket都进行token验证，只负责验证逻辑，不管理连接状态
    std::string token = message.get_token();
    std::string device_id = message.get_device_id();
    
    // 检查token是否为空
    if (token.empty()) {
        if (options_.enable_request_logging) {
            LogManager::GetLogger("coro_message_processor")
                    ->warn("CoroMessageProcessor::coro_verify_authentication: empty token for protocol: {}", 
                           static_cast<int>(message.get_protocol()));
        }
        co_return false;
    }
    
    // 这里可以添加异步的Token验证逻辑，比如查询Redis缓存
    // 为了演示，这里添加一个小的延迟模拟异步验证过程
    if (options_.enable_request_logging) {
        co_await DelayAwaiter(std::chrono::milliseconds(1)); // 模拟异步验证延迟
    }
    
    // 验证访问令牌
    bool is_valid = auth_mgr_->verify_access_token(token, device_id);
    
    if (!is_valid && options_.enable_request_logging) {
        LogManager::GetLogger("coro_message_processor")
                ->warn("CoroMessageProcessor::coro_verify_authentication: invalid token for device: {} protocol: {}", 
                       device_id, static_cast<int>(message.get_protocol()));
    }
    
    co_return is_valid;
}

/**
 * @brief 应用超时控制的协程包装器实现
 */
Task<CoroProcessorResult> CoroMessageProcessor::apply_timeout_control(
    Task<CoroProcessorResult> task, 
    std::chrono::milliseconds timeout) {
    
    // 简化的超时实现 - 在协程中实现基本的超时检查
    // 注意：这是一个演示实现，生产环境中应该使用更完善的协程组合器
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // 执行原始任务
        auto result = co_await std::move(task);
        
        // 检查是否超时
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        
        if (elapsed_ms > timeout) {
            LogManager::GetLogger("coro_message_processor")
                    ->warn("CoroMessageProcessor::apply_timeout_control: task completed after timeout, "
                           "elapsed={}ms, timeout={}ms", 
                           elapsed_ms.count(), timeout.count());
            
            // 虽然任务完成了，但已经超时，返回超时错误
            co_return CoroProcessorResult(ErrorCode::TIMEOUT, 
                                        "Request timeout after " + std::to_string(timeout.count()) + "ms");
        }
        
        if (options_.enable_performance_monitoring) {
            LogManager::GetLogger("coro_message_processor")
                    ->debug("CoroMessageProcessor::apply_timeout_control: task completed in {}ms", 
                           elapsed_ms.count());
        }
        
        co_return result;
        
    } catch (const std::exception& e) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        
        LogManager::GetLogger("coro_message_processor")
                ->error("CoroMessageProcessor::apply_timeout_control: task failed after {}ms, error: {}", 
                       elapsed_ms.count(), e.what());
        
        co_return CoroProcessorResult(ErrorCode::SERVER_ERROR, 
                                    "Task failed: " + std::string(e.what()));
    }
}

/**
 * @brief 记录处理性能指标
 */
void CoroMessageProcessor::record_performance_metrics(uint32_t cmd_id, 
                                                     std::chrono::milliseconds duration, 
                                                     bool success) {
    LogManager::GetLogger("coro_message_processor")
            ->info("CoroMessageProcessor::record_performance_metrics: cmd_id={}, duration={}ms, success={}, "
                   "active_tasks={}", 
                   cmd_id, duration.count(), success, active_task_count_.load());
    
    // 这里可以集成更完善的性能监控系统，如Prometheus指标
    // 例如：
    // - 处理时间分布统计
    // - 成功率统计
    // - 并发数监控
    // - 错误类型统计等
}

}  // namespace im::gateway