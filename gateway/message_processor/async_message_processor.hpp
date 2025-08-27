#ifndef ASYNC_MESSAGE_PROCESSOR_HPP
#define ASYNC_MESSAGE_PROCESSOR_HPP

/******************************************************************************
 *
 * @file       async_message_processor.hpp
 * @brief      异步消息处理器 - 基于Future+回调链的混合模式
 *
 * @details    整合认证管理器和路由管理器，提供高性能的异步消息处理能力
 *             支持多种处理模式：Future并发、回调链、协程风格
 *             根据cmd_id路由到不同的微服务，实现灵活的业务逻辑处理
 *
 * @author     myself
 * @date       2025/08/27
 * @version    1.0.0
 *
 *****************************************************************************/

#include <future>
#include <memory>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include <string>

#include "unified_message.hpp"
#include "../auth/auth_mgr.hpp"
#include "../router/router_mgr.hpp"

namespace im {
namespace gateway {

// ============================================================================
// 结果类型定义
// ============================================================================

/**
 * @brief 认证结果
 */
struct AuthResult {
    bool success;                ///< 认证是否成功
    UserTokenInfo user_info;     ///< 用户信息
    std::string error_message;   ///< 错误信息
    
    static AuthResult success_result(const UserTokenInfo& info) {
        return {true, info, ""};
    }
    
    static AuthResult failed_result(const std::string& error) {
        return {false, {}, error};
    }
};

/**
 * @brief 服务调用结果
 */
struct ServiceResult {
    bool success;              ///< 调用是否成功
    std::string response_data; ///< 响应数据
    int status_code;           ///< HTTP状态码
    std::string error_message; ///< 错误信息
    
    static ServiceResult success_result(const std::string& data) {
        return {true, data, 200, ""};
    }
    
    static ServiceResult failed_result(const std::string& error, int code = 500) {
        return {false, "", code, error};
    }
};

/**
 * @brief 消息处理结果
 */
struct ProcessResult {
    enum Status { 
        SUCCESS,          ///< 处理成功
        AUTH_FAILED,      ///< 认证失败
        SERVICE_ERROR,    ///< 服务调用错误
        ROUTE_NOT_FOUND,  ///< 路由未找到
        TIMEOUT,          ///< 超时
        UNKNOWN_COMMAND   ///< 未知命令
    };
    
    Status status;                                    ///< 处理状态
    std::string message;                              ///< 结果消息
    std::chrono::milliseconds processing_time;       ///< 处理耗时
    std::string trace_id;                             ///< 链路追踪ID
    
    static ProcessResult success(const std::string& msg = "Success") {
        return {SUCCESS, msg, std::chrono::milliseconds(0), ""};
    }
    
    static ProcessResult auth_failed(const std::string& msg) {
        return {AUTH_FAILED, msg, std::chrono::milliseconds(0), ""};
    }
    
    static ProcessResult service_error(const std::string& msg) {
        return {SERVICE_ERROR, msg, std::chrono::milliseconds(0), ""};
    }
    
    static ProcessResult route_not_found(const std::string& msg) {
        return {ROUTE_NOT_FOUND, msg, std::chrono::milliseconds(0), ""};
    }
    
    static ProcessResult unknown_command(uint32_t cmd_id) {
        return {UNKNOWN_COMMAND, "Unknown command: " + std::to_string(cmd_id), 
                std::chrono::milliseconds(0), ""};
    }
};

// ============================================================================
// 回调函数类型定义
// ============================================================================

using AuthCallback = std::function<void(const AuthResult&)>;
using ServiceCallback = std::function<void(const ServiceResult&)>;
using ResponseCallback = std::function<void(const ProcessResult&)>;

/**
 * @brief 命令处理器类型
 * 
 * @param message 统一消息对象
 * @param auth_result 认证结果
 * @param callback 服务调用完成回调
 */
using CommandHandler = std::function<void(
    const UnifiedMessage& message,
    const AuthResult& auth_result,
    ServiceCallback callback
)>;

// ============================================================================
// 异步消息处理器
// ============================================================================

/**
 * @class AsyncMessageProcessor
 * @brief 异步消息处理器
 * 
 * @details 基于Future+回调链的混合异步处理模式，支持：
 *          - 高并发消息处理
 *          - 灵活的命令路由
 *          - 可扩展的业务逻辑
 *          - 完整的错误处理
 *          - 性能监控和统计
 */
class AsyncMessageProcessor {
public:
    /**
     * @brief 构造函数
     * 
     * @param auth_mgr 认证管理器
     * @param router_mgr 路由管理器
     * @param thread_pool_size 线程池大小，默认为硬件并发数
     */
    explicit AsyncMessageProcessor(
        std::shared_ptr<AuthManager> auth_mgr,
        std::shared_ptr<RouterManager> router_mgr,
        size_t thread_pool_size = std::thread::hardware_concurrency()
    );
    
    /**
     * @brief 析构函数
     */
    ~AsyncMessageProcessor();

    // ===== 主要处理接口 =====

    /**
     * @brief 异步处理消息（推荐接口）
     * 
     * @param message 统一消息对象
     * @return 处理结果的future对象
     * 
     * @details 使用Future+回调链混合模式，提供最佳性能和灵活性
     */
    std::future<ProcessResult> process_message_async(
        std::unique_ptr<UnifiedMessage> message
    );

    /**
     * @brief 同步处理消息（兼容接口）
     * 
     * @param message 统一消息对象
     * @return 处理结果
     * 
     * @details 内部调用异步版本并等待结果，主要用于测试和兼容性
     */
    ProcessResult process_message_sync(
        std::unique_ptr<UnifiedMessage> message
    );

    // ===== 命令处理器管理 =====

    /**
     * @brief 注册命令处理器
     * 
     * @param cmd_id 命令ID
     * @param handler 处理器函数
     * 
     * @details 为特定命令注册自定义处理逻辑
     */
    void register_command_handler(uint32_t cmd_id, CommandHandler handler);

    /**
     * @brief 取消注册命令处理器
     * 
     * @param cmd_id 命令ID
     */
    void unregister_command_handler(uint32_t cmd_id);

    /**
     * @brief 检查命令是否已注册
     * 
     * @param cmd_id 命令ID
     * @return true 已注册，false 未注册
     */
    bool is_command_registered(uint32_t cmd_id) const;

    // ===== 配置管理 =====

    /**
     * @brief 配置结构体
     */
    struct Config {
        size_t max_concurrent_requests = 1000;    ///< 最大并发请求数
        std::chrono::milliseconds auth_timeout = std::chrono::milliseconds(5000);     ///< 认证超时
        std::chrono::milliseconds service_timeout = std::chrono::milliseconds(10000); ///< 服务调用超时
        std::chrono::milliseconds response_timeout = std::chrono::milliseconds(3000); ///< 响应超时
        bool enable_trace = true;                 ///< 启用链路追踪
        bool enable_metrics = true;               ///< 启用性能指标
    };

    /**
     * @brief 更新配置
     * 
     * @param config 新配置
     */
    void update_config(const Config& config);

    /**
     * @brief 获取当前配置
     * 
     * @return 当前配置
     */
    const Config& get_config() const { return config_; }

    // ===== 统计信息 =====

    /**
     * @brief 处理器统计信息
     */
    struct Statistics {
        std::atomic<uint64_t> total_processed{0};        ///< 总处理数
        std::atomic<uint64_t> success_count{0};          ///< 成功数
        std::atomic<uint64_t> auth_failed_count{0};      ///< 认证失败数
        std::atomic<uint64_t> service_error_count{0};    ///< 服务错误数
        std::atomic<uint64_t> timeout_count{0};          ///< 超时数
        std::atomic<uint64_t> unknown_command_count{0};  ///< 未知命令数
        
        std::atomic<uint64_t> avg_processing_time_ms{0}; ///< 平均处理时间(毫秒)
        std::atomic<uint64_t> max_processing_time_ms{0}; ///< 最大处理时间(毫秒)
        std::atomic<uint64_t> min_processing_time_ms{UINT64_MAX}; ///< 最小处理时间(毫秒)
        
        std::atomic<uint32_t> current_concurrent_requests{0}; ///< 当前并发请求数
        std::atomic<uint32_t> max_concurrent_requests{0};     ///< 最大并发请求数
    };

    /**
     * @brief 获取统计信息
     * 
     * @return 统计信息快照
     */
    Statistics get_statistics() const;

    /**
     * @brief 重置统计信息
     */
    void reset_statistics();

    /**
     * @brief 打印统计信息
     */
    void print_statistics() const;

private:
    // ===== 核心组件 =====
    std::shared_ptr<AuthManager> auth_mgr_;           ///< 认证管理器
    std::shared_ptr<RouterManager> router_mgr_;       ///< 路由管理器
    std::unordered_map<uint32_t, CommandHandler> command_handlers_; ///< 命令处理器映射
    
    // ===== 配置和状态 =====
    Config config_;                                   ///< 处理器配置
    mutable Statistics stats_;                        ///< 统计信息
    std::atomic<bool> is_running_{true};              ///< 运行状态
    
    // ===== 私有方法 =====

    /**
     * @brief 开始异步处理链
     */
    void start_async_chain(
        std::unique_ptr<UnifiedMessage> message,
        std::shared_ptr<std::promise<ProcessResult>> promise,
        std::shared_ptr<std::chrono::steady_clock::time_point> start_time
    );

    /**
     * @brief 异步认证
     */
    void verify_token_async(
        const std::string& token,
        AuthCallback callback
    );

    /**
     * @brief 异步发送响应
     */
    void send_response_async(
        const UnifiedMessage& message,
        const ServiceResult& service_result,
        ResponseCallback callback
    );

    /**
     * @brief 生成链路追踪ID
     */
    std::string generate_trace_id() const;

    /**
     * @brief 更新统计信息
     */
    void update_statistics(
        const ProcessResult& result,
        std::chrono::milliseconds processing_time
    );

    /**
     * @brief 注册默认命令处理器
     */
    void register_default_handlers();

    /**
     * @brief 检查是否需要认证
     */
    bool requires_authentication(uint32_t cmd_id) const;

    // ===== 默认处理器 =====
    
    /**
     * @brief 登录处理器
     */
    void handle_login_command(
        const UnifiedMessage& message,
        const AuthResult& auth_result,
        ServiceCallback callback
    );

    /**
     * @brief 聊天消息处理器
     */
    void handle_chat_command(
        const UnifiedMessage& message,
        const AuthResult& auth_result,
        ServiceCallback callback
    );

    /**
     * @brief 好友相关处理器
     */
    void handle_friend_command(
        const UnifiedMessage& message,
        const AuthResult& auth_result,
        ServiceCallback callback
    );

    /**
     * @brief 通用服务调用
     */
    void call_microservice_async(
        const std::string& service_name,
        const std::string& endpoint,
        const std::string& request_data,
        ServiceCallback callback
    );
};

} // namespace gateway
} // namespace im

#endif // ASYNC_MESSAGE_PROCESSOR_HPP