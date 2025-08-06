#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include "common/network/websocket_server.hpp"
#include "common/network/IOService_pool.hpp"
#include "common/utils/config_mgr.hpp"
#include "common/utils/log_manager.hpp"
#include "common/utils/json.hpp"

// 第三方HTTP库
#include "httplib.h"

// 网关核心模块
#include "connection_manager.hpp"
#include "auth_manager.hpp"
#include "route_manager.hpp"
#include "message_processor.hpp"

using json = nlohmann::json;

/**
 * @brief 网关主服务类 - 整合所有核心模块
 * 
 * 主要功能：
 * 1. 管理 WebSocket 和 HTTP 服务器
 * 2. 协调各个核心模块的工作
 * 3. 处理客户端请求和响应
 * 4. 提供统一的服务入口
 */
class GatewayServer {
public:
    GatewayServer();
    ~GatewayServer();
    
    /**
     * @brief 启动网关服务
     * @param config_path 配置文件路径
     * @return 是否启动成功
     */
    bool Start(const std::string& config_path);
    
    /**
     * @brief 停止网关服务
     */
    void Stop();
    
    /**
     * @brief 获取服务运行状态
     * @return 是否正在运行
     */
    bool IsRunning() const { return is_running_; }
    
    /**
     * @brief 获取服务统计信息
     * @return 统计信息JSON字符串
     */
    std::string GetServerStats() const;

private:
    // ==================== 初始化方法 ====================
    
    /**
     * @brief 加载配置文件
     * @param config_path 配置文件路径
     * @return 是否加载成功
     */
    bool LoadConfig(const std::string& config_path);
    
    /**
     * @brief 初始化网络服务
     * @return 是否初始化成功
     */
    bool InitializeNetwork();
    
    /**
     * @brief 初始化核心模块
     * @return 是否初始化成功
     */
    bool InitializeManagers();
    
    /**
     * @brief 初始化HTTP路由
     * @return 是否初始化成功
     */
    bool InitializeHttpRoutes();
    
    // ==================== WebSocket 处理方法 ====================
    
    /**
     * @brief WebSocket连接建立回调
     * @param session WebSocket会话
     */
    void OnWebSocketConnect(std::shared_ptr<WebSocketSession> session);
    
    /**
     * @brief WebSocket连接断开回调
     * @param session WebSocket会话
     */
    void OnWebSocketDisconnect(std::shared_ptr<WebSocketSession> session);
    
    /**
     * @brief WebSocket消息处理
     * @param session WebSocket会话
     * @param message 消息内容
     */
    void OnWebSocketMessage(std::shared_ptr<WebSocketSession> session, 
                           const std::string& message);
    
    // ==================== HTTP 处理方法 ====================
    
    /**
     * @brief HTTP登录请求处理
     * @param req HTTP请求
     * @param res HTTP响应
     */
    void HandleLoginRequest(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief HTTP登出请求处理
     * @param req HTTP请求
     * @param res HTTP响应
     */
    void HandleLogoutRequest(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief HTTP用户信息请求处理
     * @param req HTTP请求
     * @param res HTTP响应
     */
    void HandleUserInfoRequest(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief HTTP发送消息请求处理
     * @param req HTTP请求
     * @param res HTTP响应
     */
    void HandleSendMessageRequest(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief HTTP健康检查处理
     * @param req HTTP请求
     * @param res HTTP响应
     */
    void HandleHealthCheck(const httplib::Request& req, httplib::Response& res);
    
    /**
     * @brief HTTP统计信息处理
     * @param req HTTP请求
     * @param res HTTP响应
     */
    void HandleStatsRequest(const httplib::Request& req, httplib::Response& res);
    
    // ==================== 通用处理方法 ====================
    
    /**
     * @brief 处理用户登录逻辑
     * @param username 用户名
     * @param password 密码
     * @param device_id 设备ID
     * @param session_id 会话ID（可选，WebSocket登录时使用）
     * @return 登录结果
     */
    im::base::BaseResponse ProcessLogin(const std::string& username,
                                       const std::string& password,
                                       const std::string& device_id,
                                       const std::string& session_id = "");
    
    /**
     * @brief 处理用户登出逻辑
     * @param token 用户Token
     * @return 登出结果
     */
    im::base::BaseResponse ProcessLogout(const std::string& token);
    
    /**
     * @brief 验证并获取用户信息
     * @param token 用户Token
     * @param user_info 输出的用户信息
     * @return 是否验证成功
     */
    bool VerifyTokenAndGetUser(const std::string& token, UserTokenInfo& user_info);
    
    /**
     * @brief 从HTTP请求头中获取Token
     * @param req HTTP请求
     * @return Token字符串，如果没有返回空字符串
     */
    std::string ExtractTokenFromRequest(const httplib::Request& req);
    
    /**
     * @brief 设置HTTP响应的通用头部
     * @param res HTTP响应
     */
    void SetCommonHttpHeaders(httplib::Response& res);
    
    /**
     * @brief 生成服务器统计信息
     * @return 统计信息JSON
     */
    std::string GenerateServerStats() const;
    
    // ==================== 监控和维护 ====================
    
    /**
     * @brief 启动监控任务
     */
    void StartMonitoringTasks();
    
    /**
     * @brief 停止监控任务
     */
    void StopMonitoringTasks();
    
    /**
     * @brief 定期监控任务
     */
    void DoMonitoring();

private:
    // ==================== 服务组件 ====================
    
    // WebSocket服务器
    std::unique_ptr<WebSocketServer> ws_server_;
    
    // HTTP服务器
    std::unique_ptr<httplib::Server> http_server_;
    
    // IO服务池
    std::shared_ptr<IOServicePool> io_service_pool_;
    
    // HTTP服务器线程
    std::thread http_thread_;
    
    // ==================== 核心模块 ====================
    
    // 连接管理器
    std::unique_ptr<ConnectionManager> conn_mgr_;
    
    // 认证管理器
    std::unique_ptr<AuthManager> auth_mgr_;
    
    // 路由管理器
    std::unique_ptr<RouteManager> route_mgr_;
    
    // 消息处理器
    std::unique_ptr<MessageProcessor> msg_processor_;
    
    // ==================== 配置和日志 ====================
    
    // 配置管理器
    std::shared_ptr<ConfigMgr> config_mgr_;
    
    // 日志管理器
    std::shared_ptr<LogManager> log_mgr_;
    
    // 配置信息
    json gateway_config_;
    
    // ==================== 服务器配置 ====================
    
    std::string http_host_;             // HTTP服务主机
    int http_port_;                     // HTTP服务端口
    int websocket_port_;                // WebSocket服务端口
    int worker_threads_;                // 工作线程数
    int max_connections_;               // 最大连接数
    
    // ==================== 运行状态 ====================
    
    std::atomic<bool> is_running_;      // 服务运行状态
    std::atomic<bool> monitoring_running_; // 监控任务运行状态
    std::thread monitoring_thread_;     // 监控线程
    
    // ==================== 统计信息 ====================
    
    std::atomic<size_t> total_requests_;       // 总请求数
    std::atomic<size_t> websocket_requests_;   // WebSocket请求数
    std::atomic<size_t> http_requests_;        // HTTP请求数
    std::atomic<size_t> login_requests_;       // 登录请求数
    std::atomic<size_t> successful_logins_;    // 成功登录数
    
    std::chrono::system_clock::time_point start_time_; // 服务启动时间
};