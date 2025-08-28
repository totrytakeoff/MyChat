/******************************************************************************
 *
 * @file       message_processor.cpp
 * @brief      异步消息处理器的实现
 *
 * @details    实现基于std::future的异步消息处理逻辑，包括：
 *             - Token认证验证
 *             - 消息路由和处理函数调用
 *             - 异步执行和错误处理
 *
 * @author     myself
 * @date       2025/08/27
 * @version    1.0.0
 *
 *****************************************************************************/

#include "../../common/utils/log_manager.hpp"
#include "message_processor.hpp"
#include <memory>

namespace im::gateway {

using im::base::ErrorCode;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::RouterManager;
using im::gateway::UnifiedMessage;
using im::utils::LogManager;

/**
 * @brief 构造函数实现（使用现有管理器实例）
 * @details 直接使用传入的路由管理器和认证管理器实例，适用于多个组件共享管理器的场景
 */
MessageProcessor::MessageProcessor(std::shared_ptr<RouterManager> router_mgr,
                                   std::shared_ptr<MultiPlatformAuthManager> auth_mgr)
        : router_mgr_(router_mgr), auth_mgr_(auth_mgr) {
    LogManager::GetLogger("message_processor")
            ->info("MessageProcessor initialized with existing RouterManager and AuthManager");
}

/**
 * @brief 构造函数实现（从配置文件创建管理器）
 * @details 根据配置文件路径创建新的管理器实例，适用于独立使用的场景
 */
MessageProcessor::MessageProcessor(const std::string& router_config_file, const std::string& auth_config_file) {
    router_mgr_ = std::make_shared<RouterManager>(router_config_file);
    auth_mgr_ = std::make_shared<MultiPlatformAuthManager>(auth_config_file);
    LogManager::GetLogger("message_processor")
            ->info("MessageProcessor initialized with new RouterManager and AuthManager");
}

/**
 * @brief 注册消息处理函数的实现
 * @details 验证处理函数有效性和服务配置存在性后，将处理函数注册到映射表中
 */
int MessageProcessor::register_processor(
        uint32_t cmd_id, std::function<ProcessorResult(const UnifiedMessage&)> processor) {
    // 1. 检查processor是否有效
    if (!processor) {
        LogManager::GetLogger("message_processor")
                ->error("MessageProcessor::register_processor: invalid processor for cmd_id: {}",
                        cmd_id);
        return -2;
    }
    
    // 2. 验证路由配置中是否存在对应的服务
    auto service = router_mgr_->find_service(cmd_id);
    if (!service || !service->is_valid) {
        LogManager::GetLogger("message_processor")
                ->error("MessageProcessor::register_processor: service not found for cmd_id: {}",
                        cmd_id);
        return -1;
    }
    
    // 3. 检查是否已经注册过该cmd_id的处理函数（需要C++20支持）
    if (processor_map_.contains(cmd_id)) {
        LogManager::GetLogger("message_processor")
                ->warn("MessageProcessor::register_processor: processor already exists for cmd_id: {}",
                       cmd_id);
        return 1;
    }
    
    // 4. 注册处理函数
    processor_map_[cmd_id] = processor;
    LogManager::GetLogger("message_processor")
            ->info("MessageProcessor::register_processor: processor registered for cmd_id: {}",
                   cmd_id);
    return 0;
}

/**
 * @brief 异步处理消息的实现
 * @details 在独立线程中执行完整的消息处理流程，包括认证验证、处理函数查找和执行
 */
std::future<ProcessorResult> MessageProcessor::process_message(
        std::unique_ptr<UnifiedMessage> message) {
    return std::async(
            std::launch::async, [this, message = std::move(message)]() mutable -> ProcessorResult {
                try {
                    // 1. 验证消息有效性
                    if (!message) {
                        LogManager::GetLogger("message_processor")
                                ->error("MessageProcessor::process_message: null message");
                        return ProcessorResult(ErrorCode::INVALID_REQUEST, "Invalid message");
                    }

                    // 2. 协议差异化处理：HTTP每次请求都进行token验证，WebSocket只在连接时验证
                    if (message->get_protocol() == UnifiedMessage::Protocol::HTTP) {
                        std::string token = message->get_token();
                        std::string device_id = message->get_device_id();
                        
                        // 验证访问令牌
                        if (!auth_mgr_->verify_access_token(token, device_id)) {
                            LogManager::GetLogger("message_processor")
                                    ->warn("MessageProcessor::process_message: invalid token for device: {}", 
                                           device_id);
                            return ProcessorResult(ErrorCode::AUTH_FAILED, "Invalid token");
                        }
                    }

                    // 3. 查找对应的处理函数（可使用C++20的contains方法优化）
                    uint32_t cmd_id = message->get_cmd_id();
                    auto processor_it = processor_map_.find(cmd_id);
                    
                    if (processor_it == processor_map_.end()) {
                        LogManager::GetLogger("message_processor")
                                ->error("MessageProcessor::process_message: no processor for cmd_id: {}",
                                        cmd_id);
                        return ProcessorResult(ErrorCode::NOT_FOUND, 
                                             "Unknown command: " + std::to_string(cmd_id));
                    }
                    
                    // 4. 执行注册的处理函数
                    LogManager::GetLogger("message_processor")
                            ->debug("MessageProcessor::process_message: executing processor for cmd_id: {}", 
                                   cmd_id);
                    return processor_it->second(*message);

                } catch (const std::exception& e) {
                    // 5. 异常处理和日志记录
                    LogManager::GetLogger("message_processor")
                            ->error("MessageProcessor::process_message: exception occurred: {}", e.what());
                    return ProcessorResult(ErrorCode::SERVER_ERROR,
                                           std::string("Exception: ") + e.what());
                }
            });
}



}  // namespace im::gateway
