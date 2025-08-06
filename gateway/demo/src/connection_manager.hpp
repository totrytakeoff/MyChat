#pragma once

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <mutex>
#include <chrono>
#include "common/network/websocket_session.hpp"
#include "common/utils/log_manager.hpp"
#include "common/utils/global.hpp"

/**
 * @brief 统一的网关会话接口
 * 
 * 为WebSocket和HTTP连接提供统一的接口，方便连接管理器统一管理
 */
class IGatewaySession {
public:
    virtual ~IGatewaySession() = default;
    
    // 纯虚函数 - 子类必须实现
    virtual void SendMessage(const std::string& data) = 0;
    virtual void Close() = 0;
    virtual std::string GetSessionId() const = 0;
    virtual std::string GetRemoteAddress() const = 0;
    virtual bool IsConnected() const = 0;
    
    // 用户绑定信息
    const std::string& GetUserId() const { return user_id_; }
    void SetUserId(const std::string& user_id) { user_id_ = user_id; }
    
    bool IsAuthenticated() const { return authenticated_; }
    void SetAuthenticated(bool auth) { authenticated_ = auth; }
    
    // 连接时间和心跳
    std::chrono::system_clock::time_point GetConnectTime() const { return connect_time_; }
    std::chrono::system_clock::time_point GetLastHeartbeat() const { return last_heartbeat_; }
    void UpdateHeartbeat() { last_heartbeat_ = std::chrono::system_clock::now(); }
    
    // 会话统计信息
    void IncrementMessageCount() { ++message_count_; }
    uint64_t GetMessageCount() const { return message_count_; }

protected:
    std::string user_id_;                                           // 绑定的用户ID
    bool authenticated_ = false;                                    // 是否已认证
    std::chrono::system_clock::time_point connect_time_ = 
        std::chrono::system_clock::now();                          // 连接时间
    std::chrono::system_clock::time_point last_heartbeat_ = 
        std::chrono::system_clock::now();                          // 最后心跳时间
    uint64_t message_count_ = 0;                                   // 消息计数
};

/**
 * @brief WebSocket会话包装器
 */
class GatewayWebSocketSession : public IGatewaySession {
public:
    explicit GatewayWebSocketSession(std::shared_ptr<WebSocketSession> ws_session);
    ~GatewayWebSocketSession() override = default;
    
    void SendMessage(const std::string& data) override;
    void Close() override;
    std::string GetSessionId() const override;
    std::string GetRemoteAddress() const override;
    bool IsConnected() const override;
    
    // 获取底层WebSocket会话（用于特殊操作）
    std::shared_ptr<WebSocketSession> GetWebSocketSession() const { return ws_session_.lock(); }

private:
    std::weak_ptr<WebSocketSession> ws_session_;  // 使用weak_ptr避免循环引用
    std::string session_id_;
};

/**
 * @brief 连接管理器 - 统一管理所有客户端连接
 * 
 * 主要功能：
 * 1. 管理WebSocket长连接
 * 2. 维护用户ID与连接的映射关系
 * 3. 提供连接查找、广播等功能
 * 4. 连接状态监控和清理
 */
class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();
    
    // ==================== 连接管理 ====================
    
    /**
     * @brief 添加新的会话连接
     * @param session 会话指针
     * @return 是否添加成功
     */
    bool AddSession(std::shared_ptr<IGatewaySession> session);
    
    /**
     * @brief 移除会话连接
     * @param session_id 会话ID
     */
    void RemoveSession(const std::string& session_id);
    
    /**
     * @brief 根据会话ID查找会话
     * @param session_id 会话ID
     * @return 会话指针，如果不存在返回nullptr
     */
    std::shared_ptr<IGatewaySession> FindSessionById(const std::string& session_id);
    
    // ==================== 用户绑定管理 ====================
    
    /**
     * @brief 将用户ID绑定到会话
     * @param user_id 用户ID
     * @param session_id 会话ID
     * @return 是否绑定成功
     */
    bool BindUserToSession(const std::string& user_id, const std::string& session_id);
    
    /**
     * @brief 解绑用户
     * @param user_id 用户ID
     */
    void UnbindUser(const std::string& user_id);
    
    /**
     * @brief 根据用户ID查找会话
     * @param user_id 用户ID
     * @return 会话指针，如果用户不在线返回nullptr
     */
    std::shared_ptr<IGatewaySession> FindSessionByUser(const std::string& user_id);
    
    /**
     * @brief 检查用户是否在线
     * @param user_id 用户ID
     * @return 是否在线
     */
    bool IsUserOnline(const std::string& user_id);
    
    // ==================== 消息推送 ====================
    
    /**
     * @brief 向指定用户推送消息
     * @param user_id 用户ID
     * @param message 消息内容
     * @return 是否推送成功
     */
    bool PushMessageToUser(const std::string& user_id, const std::string& message);
    
    /**
     * @brief 向多个用户推送消息
     * @param user_ids 用户ID列表
     * @param message 消息内容
     * @return 成功推送的用户数量
     */
    size_t PushMessageToUsers(const std::vector<std::string>& user_ids, 
                             const std::string& message);
    
    /**
     * @brief 向所有在线用户广播消息
     * @param message 消息内容
     * @return 成功推送的用户数量
     */
    size_t BroadcastMessage(const std::string& message);
    
    // ==================== 统计和监控 ====================
    
    /**
     * @brief 获取总连接数
     * @return 连接数
     */
    size_t GetTotalSessions() const;
    
    /**
     * @brief 获取已认证连接数
     * @return 已认证连接数
     */
    size_t GetAuthenticatedSessions() const;
    
    /**
     * @brief 获取在线用户数
     * @return 在线用户数
     */
    size_t GetOnlineUserCount() const;
    
    /**
     * @brief 获取所有在线用户ID列表
     * @return 用户ID列表
     */
    std::vector<std::string> GetOnlineUserList() const;
    
    /**
     * @brief 获取连接统计信息
     * @return 统计信息的JSON字符串
     */
    std::string GetConnectionStats() const;
    
    // ==================== 连接清理 ====================
    
    /**
     * @brief 清理无效连接
     * @return 清理的连接数
     */
    size_t CleanupInvalidSessions();
    
    /**
     * @brief 清理超时连接（超过指定时间未收到心跳）
     * @param timeout_seconds 超时时间（秒）
     * @return 清理的连接数
     */
    size_t CleanupTimeoutSessions(int timeout_seconds = 300);
    
    /**
     * @brief 启动定时清理任务
     */
    void StartCleanupTask();
    
    /**
     * @brief 停止定时清理任务
     */
    void StopCleanupTask();

private:
    // 线程安全保护
    mutable std::mutex mutex_;
    
    // 会话存储：session_id -> session
    std::unordered_map<std::string, std::shared_ptr<IGatewaySession>> sessions_;
    
    // 用户映射：user_id -> session_id
    std::unordered_map<std::string, std::string> user_session_map_;
    
    // 日志管理器
    std::shared_ptr<LogManager> log_mgr_;
    
    // 清理任务控制
    std::atomic<bool> cleanup_running_;
    std::thread cleanup_thread_;
    
    // 内部辅助方法
    void DoCleanup();
    std::string GenerateSessionStats() const;
};

/**
 * @brief 连接管理器单例
 */
using ConnectionManagerPtr = std::shared_ptr<ConnectionManager>;