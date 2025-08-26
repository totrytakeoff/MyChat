#ifndef MESSAGE_PARSER_HPP
#define MESSAGE_PARSER_HPP

/******************************************************************************
 *
 * @file       message_parser.hpp
 * @brief      统一消息解析器 - 基于RouterManager的重构版本
 * 
 * @details    将HTTP请求和WebSocket消息解析转换为统一的UnifiedMessage格式
 *             专注于协议解析和消息格式转换，不涉及具体业务处理逻辑
 *             为上层MessageProcessor提供统一的消息格式输入
 *
 * @author     myself
 * @date       2025/08/22
 * @version    2.0.0
 *
 *****************************************************************************/

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <shared_mutex>
#include <stdexcept>

#include <httplib.h>
#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/utils/log_manager.hpp"
#include "../router/router_mgr.hpp"
#include "unified_message.hpp"


namespace im {
namespace gateway {

using im::gateway::RouterManager;
using im::gateway::UnifiedMessage;

/**
 * @brief 解析结果结构体
 * 
 * @details 提供详细的解析结果信息，包括成功/失败状态和错误详情
 */
struct ParseResult {
    bool success;                                    ///< 处理是否成功
    std::unique_ptr<UnifiedMessage> message;         ///< 处理成功时的消息对象
    std::string error_message;                       ///< 失败时的错误信息
    int error_code;                                  ///< 错误代码
    
    // 错误代码常量
    static constexpr int SUCCESS = 0;
    static constexpr int ROUTING_FAILED = 1001;
    static constexpr int DECODE_FAILED = 1002;
    static constexpr int INVALID_REQUEST = 1003;
    static constexpr int PARSE_ERROR = 1999;
    
    // 便利构造函数
    static ParseResult success_result(std::unique_ptr<UnifiedMessage> msg) {
        return {true, std::move(msg), "", SUCCESS};
    }
    
    static ParseResult error_result(int code, const std::string& message) {
        return {false, nullptr, message, code};
    }
};
/**
 * @brief 统一消息解析器
 * 
 * @details 基于RouterManager重构的消息解析器，负责将HTTP请求和WebSocket消息
 *          解析转换为统一的UnifiedMessage格式，专注于协议解析和消息格式转换
 */
class MessageParser {
public:
    /**
     * @brief 构造函数
     * 
     * @param config_file 路由配置文件路径
     * 
     * @throws std::runtime_error 当RouterManager初始化失败或配置文件无效时抛出
     * @throws std::invalid_argument 当配置文件路径为空时抛出
     * 
     * @details 创建消息解析器实例并初始化RouterManager，验证配置有效性
     */
    explicit MessageParser(const std::string& config_file);

    /**
     * @brief 重新加载配置
     * 
     * @return true 成功，false 失败
     * 
     * @details 支持运行时动态重新加载路由配置
     */
    bool reload_config();

    /**
     * @brief 解析HTTP请求
     * 
     * @param req HTTP请求对象
     * @param session_id 会话ID（可选）
     * @return 统一消息对象，失败返回nullptr
     * 
     * @details 将HTTP请求解析转换为UnifiedMessage，包含消息内容和会话信息
     */
    std::unique_ptr<UnifiedMessage> parse_http_request(
        const httplib::Request& req,
        const std::string& session_id = ""
    );
    
    /**
     * @brief 解析HTTP请求（增强版本）
     * 
     * @param req HTTP请求对象
     * @param session_id 会话ID（可选）
     * @return 解析结果，包含详细的错误信息
     * 
     * @details 提供详细错误信息的HTTP请求解析版本
     */
    ParseResult parse_http_request_enhanced(
        const httplib::Request& req,
        const std::string& session_id = ""
    );

    /**
     * @brief 解析WebSocket消息
     * 
     * @param raw_message 原始二进制消息
     * @param session_id 会话ID（可选）
     * @return 统一消息对象，失败返回nullptr
     * 
     * @details 解码Protobuf消息并解析转换为UnifiedMessage，包含消息内容和会话信息
     */
    std::unique_ptr<UnifiedMessage> parse_websocket_message(
        const std::string& raw_message,
        const std::string& session_id = ""
    );
    
    /**
     * @brief 解析WebSocket消息（增强版本）
     * 
     * @param raw_message 原始二进制消息
     * @param session_id 会话ID（可选）
     * @return 解析结果，包含详细的错误信息
     * 
     * @details 提供详细错误信息的WebSocket消息解析版本
     */
    ParseResult parse_websocket_message_enhanced(
        const std::string& raw_message,
        const std::string& session_id = ""
    );

    /**
     * @brief 获取路由管理器引用（用于路由查询）
     * 
     * @return RouterManager引用
     * 
     * @details 提供对内部RouterManager的访问，用于路由信息查询
     */
    const RouterManager& get_router_manager() const { return *router_manager_; }

    /**
     * @brief 获取解析器统计信息
     * 
     * @return 解析器统计信息
     */
    struct ParserStats {
        size_t http_requests_parsed{0};     ///< 解析的HTTP请求数
        size_t websocket_messages_parsed{0}; ///< 解析的WebSocket消息数
        size_t routing_failures{0};           ///< 路由失败次数
        size_t decode_failures{0};            ///< 解码失败次数
        RouterManager::RouterStats router_stats; ///< 路由器统计信息
    };

    ParserStats get_stats() const;

    /**
     * @brief 重置统计信息
     */
    void reset_stats();

private:
    // 核心组件
    std::unique_ptr<RouterManager> router_manager_;         ///< 路由管理器
    im::network::ProtobufCodec protobuf_codec_;            ///< Protobuf编解码器

    // 统计信息（线程安全）
    mutable std::atomic<size_t> http_requests_parsed_{0};
    mutable std::atomic<size_t> websocket_messages_parsed_{0};
    mutable std::atomic<size_t> routing_failures_{0};
    mutable std::atomic<size_t> decode_failures_{0};
    mutable std::shared_mutex config_mutex_;               ///< 配置读写锁

    // 会话管理
    static std::atomic<uint64_t> session_counter_;          ///< 会话计数器

    // ===== 私有辅助方法 =====

    /**
     * @brief 从HTTP请求中提取认证令牌
     */
    std::string extract_token(const httplib::Request& req);

    /**
     * @brief 从HTTP请求中提取设备ID
     */
    std::string extract_device_id(const httplib::Request& req);

    /**
     * @brief 从HTTP请求中提取平台信息
     */
    std::string extract_platform(const httplib::Request& req);

    /**
     * @brief 从HTTP请求中提取客户端IP
     */
    std::string extract_client_ip(const httplib::Request& req);

    /**
     * @brief 生成唯一的会话ID（通用）
     */
    std::string generate_session_id();
    
    /**
     * @brief 为HTTP请求生成会话ID
     * 
     * @param req HTTP请求对象（用于扩展性，当前实现不使用）
     * @return HTTP会话ID字符串，使用"http_"前缀避免与WebSocket冲突
     */
    std::string generate_http_session_id(const httplib::Request& req);

    // 注意：作为解析器，只负责格式转换
    // HTTP请求的JSON数据和WebSocket的Protobuf数据都直接保存在UnifiedMessage中
    // 具体的业务处理由上层MessageProcessor根据cmd_id进行


};
}  // namespace gateway
}  // namespace im



#endif  // MESSAGE_PARSER_HPP