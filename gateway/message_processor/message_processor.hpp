#ifndef MESSAGE_PROCESSOR_HPP
#define MESSAGE_PROCESSOR_HPP

/******************************************************************************
 *
 * @file       message_processor.hpp
 * @brief      异步消息处理器，用于处理解析后的统一消息
 *
 * @details    基于Future模式的异步消息处理器，负责：
 *             1. 对HTTP协议消息进行Token认证验证
 *             2. 根据cmd_id路由到注册的处理函数
 *             3. 异步执行业务逻辑处理
 *             4. 统一的错误处理和日志记录
 *
 * @author     myself
 * @date       2025/08/27
 * @version    1.0.0
 *
 *****************************************************************************/

#include "../../common/proto/base.pb.h"
#include "../auth/multi_platform_auth.hpp"
#include "../router/router_mgr.hpp"
#include "message_parser.hpp"
#include "unified_message.hpp"

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>

namespace im::gateway {

// 类型别名，简化代码
using im::gateway::MessageParser;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::RouterManager;
using im::gateway::UnifiedMessage;

/**
 * @brief 消息处理结果结构体
 *
 * @details 封装消息处理的结果信息，包括状态码、错误信息和响应数据
 *          支持Protobuf和JSON两种响应格式
 *  json_body: {
 *     code,
 *     body,
 *     err_msg
 *  }
 */
struct ProcessorResult {
    int status_code;               ///< 状态码，0表示成功，其他表示错误类型
    std::string error_message;     ///< 错误信息描述
    std::string protobuf_message;  ///< Protobuf格式的响应数据 (header + body
                                   ///< ,需配合protobufcodec进行使用)
    std::string json_body;         ///< JSON格式的响应数据

    /**
     * @brief 默认构造函数，创建成功状态的结果
     */
    ProcessorResult() : status_code(0), error_message(), protobuf_message(), json_body() {}

    /**
     * @brief 构造错误结果
     * @param code 错误状态码
     * @param err_msg 错误信息
     */
    ProcessorResult(int code, std::string err_msg)
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
    ProcessorResult(int code, std::string err_msg, std::string pb_msg, std::string json)
            : status_code(code)
            , error_message(std::move(err_msg))
            , protobuf_message(std::move(pb_msg))
            , json_body(std::move(json)) {}
};

/**
 * @class MessageProcessor
 * @brief 异步消息处理器
 *
 * @details 基于std::future的异步消息处理器，主要功能：
 *          - 对HTTP协议消息进行Token认证（WebSocket在连接时认证）
 *          - 根据cmd_id查找并执行注册的处理函数
 *          - 提供异步处理能力，避免阻塞调用线程
 *          - 统一的错误处理和日志记录机制
 *
 * @example 典型使用方式：
 * @code
 * // 1. 创建处理器
 * auto processor = std::make_unique<MessageProcessor>(router_mgr, auth_mgr);
 *
 * // 2. 注册处理函数
 * processor->register_processor(1001, [](const UnifiedMessage& msg) {
 *     // 登录逻辑处理
 *     return ProcessorResult(0, "", "", "{\"status\":\"success\"}");
 * });
 *
 * // 3. 异步处理消息
 * auto future_result = processor->process_message(std::move(message));
 * auto result = future_result.get(); // 获取处理结果
 * @endcode
 */
class MessageProcessor {
public:
    /**
     * @brief 构造函数（使用现有的管理器实例）
     * @param router_mgr 路由管理器，用于服务发现和路由
     * @param auth_mgr 多平台认证管理器，用于Token验证
     */
    MessageProcessor(std::shared_ptr<RouterManager> router_mgr,
                     std::shared_ptr<MultiPlatformAuthManager>
                             auth_mgr);

    /**
     * @brief 构造函数（从配置文件创建管理器）
     * @param router_config_file 路由配置文件路径
     * @param auth_config_file 认证配置文件路径
     */
    MessageProcessor(const std::string& router_config_file, const std::string& auth_config_file);

    // 禁用拷贝构造和赋值
    MessageProcessor(const MessageProcessor&) = delete;
    MessageProcessor& operator=(const MessageProcessor&) = delete;


    /**
     * @brief 注册消息处理函数
     *
     * @param cmd_id 命令ID，用于路由消息到对应的处理函数
     * @param processor 处理函数，接收UnifiedMessage并返回ProcessorResult
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
    int register_processor(
            uint32_t cmd_id, std::function<ProcessorResult(const UnifiedMessage&)> processor);

    /**
     * @brief 异步处理消息
     *
     * @param message 待处理的统一消息对象
     * @return std::future<ProcessorResult> 异步处理结果
     *
     * @details 处理流程：
     *          1. 对于HTTP协议消息，验证Access Token
     *          2. 根据cmd_id查找对应的处理函数
     *          3. 在独立线程中执行处理函数
     *          4. 返回处理结果的future对象
     *
     * @note WebSocket消息不在此处验证Token，假设在连接建立时已验证
     */
    std::future<ProcessorResult> process_message(std::unique_ptr<UnifiedMessage> message);

    /**
     * @brief 获取已注册的处理函数数量
     * @return 当前注册的处理函数数量
     */
    int get_callback_count() const { return processor_map_.size(); }

private:
        bool verify_access_token(const UnifiedMessage& message);

private:
    /// 路由管理器，用于服务发现和cmd_id到服务的映射
    std::shared_ptr<RouterManager> router_mgr_;

    /// 多平台认证管理器，用于Token验证和用户身份认证
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;

    /// 消息处理函数映射表，key为cmd_id，value为对应的处理函数
    std::unordered_map<uint32_t, std::function<ProcessorResult(const UnifiedMessage&)>>
            processor_map_;
};

}  // namespace im::gateway

/**
 * @note 使用注意事项：
 * 1. 处理函数应该避免长时间阻塞操作，如需要可以在内部使用异步调用
 * 2. HTTP协议消息会自动进行Token验证，WebSocket消息假设已在连接时验证
 * 3. 需要C++20支持以使用std::unordered_map::contains方法
 * 4. 错误处理使用ErrorCode枚举，确保与系统其他模块保持一致
 * 5. 所有操作都有详细的日志记录，便于问题排查和监控
 */

#endif  // MESSAGE_PROCESSOR_HPP