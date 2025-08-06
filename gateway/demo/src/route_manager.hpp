#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include "common/proto/base.pb.h"
#include "common/proto/command.pb.h"
#include "common/utils/log_manager.hpp"
#include "common/utils/json.hpp"
#include "message_processor.hpp"

using json = nlohmann::json;

/**
 * @brief 服务配置信息
 */
struct ServiceConfig {
    std::string name;           // 服务名称
    std::string host;           // 服务主机
    int port;                   // 服务端口
    int timeout_ms;             // 超时时间（毫秒）
    bool available;             // 服务是否可用
    
    ServiceConfig() : port(0), timeout_ms(5000), available(true) {}
    
    ServiceConfig(const std::string& service_name, 
                 const std::string& service_host,
                 int service_port, 
                 int timeout = 5000)
        : name(service_name), host(service_host), port(service_port)
        , timeout_ms(timeout), available(true) {}
    
    std::string GetAddress() const {
        return host + ":" + std::to_string(port);
    }
};

/**
 * @brief 服务类型枚举
 */
enum class ServiceType {
    USER_SERVICE,       // 用户服务 (1001-1999)
    MESSAGE_SERVICE,    // 消息服务 (2001-2999)
    FRIEND_SERVICE,     // 好友服务 (3001-3999)
    GROUP_SERVICE,      // 群组服务 (4001-4999)
    PUSH_SERVICE,       // 推送服务 (5001-5999)
    UNKNOWN_SERVICE     // 未知服务
};

/**
 * @brief 路由管理器 - 负责将请求路由到对应的微服务
 * 
 * 主要功能：
 * 1. 根据命令ID将请求路由到对应服务
 * 2. 管理服务配置和状态
 * 3. 提供基础的负载均衡和故障转移
 * 4. 服务健康检查
 */
class RouteManager {
public:
    RouteManager();
    ~RouteManager();
    
    /**
     * @brief 初始化路由管理器
     * @param services_config 服务配置信息
     * @return 是否初始化成功
     */
    bool Initialize(const json& services_config);
    
    // ==================== 路由接口 ====================
    
    /**
     * @brief 路由请求到对应服务
     * @param session 客户端会话
     * @param parsed_msg 解析后的消息
     * @return 是否路由成功
     */
    bool RouteRequest(std::shared_ptr<class IGatewaySession> session, 
                     const MessageProcessor::ParsedMessage& parsed_msg);
    
    /**
     * @brief 同步调用用户服务
     * @param header 消息头
     * @param payload 请求载荷
     * @return 服务响应
     */
    im::base::BaseResponse CallUserService(const im::base::IMHeader& header,
                                          const std::string& payload);
    
    /**
     * @brief 同步调用消息服务
     * @param header 消息头
     * @param payload 请求载荷
     * @return 服务响应
     */
    im::base::BaseResponse CallMessageService(const im::base::IMHeader& header,
                                             const std::string& payload);
    
    /**
     * @brief 同步调用好友服务
     * @param header 消息头
     * @param payload 请求载荷
     * @return 服务响应
     */
    im::base::BaseResponse CallFriendService(const im::base::IMHeader& header,
                                            const std::string& payload);
    
    /**
     * @brief 同步调用群组服务
     * @param header 消息头
     * @param payload 请求载荷
     * @return 服务响应
     */
    im::base::BaseResponse CallGroupService(const im::base::IMHeader& header,
                                           const std::string& payload);
    
    // ==================== 服务管理 ====================
    
    /**
     * @brief 根据命令ID获取服务类型
     * @param cmd_id 命令ID
     * @return 服务类型
     */
    ServiceType GetServiceType(uint32_t cmd_id) const;
    
    /**
     * @brief 获取服务配置
     * @param service_type 服务类型
     * @return 服务配置，如果不存在返回nullptr
     */
    std::shared_ptr<ServiceConfig> GetServiceConfig(ServiceType service_type) const;
    
    /**
     * @brief 更新服务状态
     * @param service_type 服务类型
     * @param available 是否可用
     */
    void UpdateServiceStatus(ServiceType service_type, bool available);
    
    /**
     * @brief 检查服务是否可用
     * @param service_type 服务类型
     * @return 是否可用
     */
    bool IsServiceAvailable(ServiceType service_type) const;
    
    // ==================== 健康检查 ====================
    
    /**
     * @brief 启动服务健康检查
     */
    void StartHealthCheck();
    
    /**
     * @brief 停止服务健康检查
     */
    void StopHealthCheck();
    
    /**
     * @brief 执行单个服务的健康检查
     * @param service_type 服务类型
     * @return 健康检查结果
     */
    bool CheckServiceHealth(ServiceType service_type);
    
    // ==================== 统计和监控 ====================
    
    /**
     * @brief 获取路由统计信息
     * @return 统计信息JSON字符串
     */
    std::string GetRouteStats() const;
    
    /**
     * @brief 获取所有服务的状态
     * @return 服务状态JSON字符串
     */
    std::string GetServicesStatus() const;

private:
    // ==================== 内部方法 ====================
    
    /**
     * @brief 发送HTTP请求到服务
     * @param service_config 服务配置
     * @param endpoint 服务端点
     * @param request_body 请求体
     * @return 响应结果
     */
    im::base::BaseResponse SendHttpRequest(
        const ServiceConfig& service_config,
        const std::string& endpoint,
        const std::string& request_body) const;
    
    /**
     * @brief 处理服务响应
     * @param session 客户端会话
     * @param parsed_msg 原始消息
     * @param service_response 服务响应
     */
    void HandleServiceResponse(std::shared_ptr<class IGatewaySession> session,
                              const MessageProcessor::ParsedMessage& parsed_msg,
                              const im::base::BaseResponse& service_response);
    
    /**
     * @brief 发送错误响应给客户端
     * @param session 客户端会话
     * @param parsed_msg 原始消息
     * @param error_code 错误码
     * @param error_message 错误信息
     */
    void SendErrorResponse(std::shared_ptr<class IGatewaySession> session,
                          const MessageProcessor::ParsedMessage& parsed_msg,
                          im::base::ErrorCode error_code,
                          const std::string& error_message);
    
    /**
     * @brief 健康检查任务
     */
    void DoHealthCheck();
    
    /**
     * @brief 将命令ID转换为服务端点
     * @param cmd_id 命令ID
     * @return 服务端点路径
     */
    std::string GetServiceEndpoint(uint32_t cmd_id) const;
    
    /**
     * @brief 生成路由统计信息
     * @return 统计信息JSON
     */
    std::string GenerateRouteStats() const;
    
    /**
     * @brief 服务类型转字符串
     * @param service_type 服务类型
     * @return 字符串表示
     */
    std::string ServiceTypeToString(ServiceType service_type) const;

private:
    // 线程安全保护
    mutable std::mutex mutex_;
    
    // 服务配置映射：service_type -> config
    std::unordered_map<ServiceType, std::shared_ptr<ServiceConfig>> service_configs_;
    
    // 消息处理器（用于封装响应）
    std::unique_ptr<MessageProcessor> msg_processor_;
    
    // 日志管理器
    std::shared_ptr<LogManager> log_mgr_;
    
    // 健康检查控制
    std::atomic<bool> health_check_running_;
    std::thread health_check_thread_;
    
    // 统计信息
    std::atomic<size_t> total_requests_;           // 总请求数
    std::atomic<size_t> successful_requests_;      // 成功请求数
    std::atomic<size_t> failed_requests_;          // 失败请求数
    std::atomic<size_t> timeout_requests_;         // 超时请求数
    
    // 服务类型字符串映射
    static const std::unordered_map<ServiceType, std::string> service_type_names_;
};