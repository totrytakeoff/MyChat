#ifndef CONNECTION_MANAGER_HPP
#define CONNECTION_MANAGER_HPP

/******************************************************************************
 *
 * @file       connection_manager.hpp
 * @brief      连接管理器，负责管理客户端连接，支持多设备登录和同设备登录挤号
 *             使用Redis作为后端存储，与WebSocketServer集成实现会话管理
 *
 * @author     myself
 * @date       2025/08/19
 *
 * @details    该类提供了以下核心功能：
 *             1. 多设备登录支持：允许同一用户在不同设备上同时登录
 *             2. 平台特定配置：根据平台配置决定是否允许多设备登录
 *             3. 登录挤号机制：对于不允许多设备登录的平台，实现登录挤号功能
 *             4. Redis存储：使用Redis持久化存储连接信息，支持分布式部署
 *             5. WebSocket集成：与WebSocketServer集成，支持会话操作
 *
 * @note       Redis键结构设计：
 *             - user:sessions:{user_id} 存储用户在各个设备上的会话信息
 *             - user:platform:{user_id} 记录用户在各个平台上的设备信息
 *             - session:user:{session_id} 通过会话ID快速查找用户信息
 *
 *****************************************************************************/

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/database/redis_mgr.hpp"
#include "common/network/websocket_session.hpp"
#include "gateway/auth/multi_platform_auth.hpp"

namespace im {
namespace gateway {


using SessionPtr = network::SessionPtr;
using RedisManager = im::db::RedisManager;
using PlatformTokenStrategy = im::gateway::PlatformTokenStrategy;



// 设备会话信息结构
struct DeviceSessionInfo {
    std::string session_id;  ///< WebSocket会话ID
    std::string device_id;   ///< 设备唯一标识
    std::string platform;    ///< 平台标识（如android, ios, web等）
    std::chrono::system_clock::time_point connect_time;  ///< 连接建立时间

    /**
     * @brief 序列化为JSON格式
     * @return JSON对象
     */
    nlohmann::json to_json() const;

    /**
     * @brief 从JSON对象反序列化
     * @param j JSON对象
     * @return DeviceSessionInfo对象
     */
    static DeviceSessionInfo from_json(const nlohmann::json& j);
};

// 前置声明
class GatewayServer;

/**
 * @class ConnectionManager
 * @brief 连接管理器类，负责管理客户端WebSocket连接
 *
 * 该类使用Redis作为后端存储，支持多设备登录和同平台登录挤号功能。
 * 通过与WebSocketServer集成，实现会话的添加、查询和移除操作。
 */
class ConnectionManager {
public:
    /**
     * @brief 构造函数
     * @param platform_config_path 平台配置文件路径
     * @param websocket_server WebSocket服务器指针，用于获取会话信息
     */
    explicit ConnectionManager(const std::string& platform_config_path,
                               network::WebSocketServer* websocket_server);

    /**
     * @brief 添加连接（支持多设备）
     * @param user_id 用户ID
     * @param device_id 设备ID
     * @param platform 平台标识
     * @param session WebSocket会话
     * @return 是否添加成功
     *
     * @details 该方法会检查是否需要踢掉同平台的旧连接，然后将新的会话信息
     *          存储到Redis中，包括用户会话映射、设备到用户映射和会话到用户映射。
     */
    bool add_connection(const std::string& user_id,
                        const std::string& device_id,
                        const std::string& platform,
                        SessionPtr session);

    /**
     * @brief 移除连接（通过用户ID和设备ID）
     * @param user_id 用户ID
     * @param device_id 设备ID
     *
     * @details 从Redis中删除指定用户和设备的会话信息，包括所有相关的映射关系。
     */
    void remove_connection(const std::string& user_id, const std::string& device_id);

    /**
     * @brief 移除连接（通过会话）
     * @param session WebSocket会话
     *
     * @details 通过会话ID获取用户信息，然后调用remove_connection(user_id, device_id)方法。
     */
    void remove_connection(SessionPtr session);

    /**
     * @brief 获取指定设备的会话
     * @param user_id 用户ID
     * @param device_id 设备ID
     * @param platform 平台标识
     * @return WebSocket会话指针，如果找不到则返回nullptr
     *
     * @details 通过Redis查询会话信息，然后通过WebSocketServer获取实际的SessionPtr。
     */
    SessionPtr get_session(const std::string& user_id,
                           const std::string& device_id,
                           const std::string& platform);

    /**
     * @brief 获取用户的所有在线设备会话
     * @param user_id 用户ID
     * @return 设备会话信息列表
     *
     * @details 从Redis中获取用户的所有会话信息，返回DeviceSessionInfo对象列表。
     */
    std::vector<DeviceSessionInfo> get_user_sessions(const std::string& user_id);

    /**
     * @brief 获取在线用户列表
     * @return 在线用户ID列表
     *
     * @note 该实现需要一个全局的在线用户集合，可以考虑使用Redis的集合来维护。
     */
    std::vector<std::string> get_online_users();

    /**
     * @brief 获取在线用户数量
     * @return 在线用户数量
     */
    size_t get_online_count() const;

    /**
     * @brief 检查用户在指定平台是否已登录
     * @param user_id 用户ID
     * @param platform 平台标识
     * @return 是否已登录
     *
     * @details 查询Redis中的用户平台设备集合，检查是否有指定平台的设备。
     */
    bool is_user_online_on_platform(const std::string& user_id, const std::string& platform);

private:
    /**
     * @brief 检查是否需要踢掉同平台的旧连接
     * @param user_id 用户ID
     * @param device_id 设备ID
     * @param platform 平台标识
     * @return 被踢掉的会话ID，如果没有则为空
     *
     * @details 根据平台配置决定是否允许多设备登录，如果不允许则踢掉同平台的旧连接。
     */
    std::string check_and_kick_same_platform(const std::string& user_id,
                                             const std::string& device_id,
                                             const std::string& platform);

    /**
     * @brief 断开指定会话
     * @param session_id 会话ID
     *
     * @details 通过WebSocketServer获取会话并关闭连接。
     */
    void disconnect_session(const std::string& session_id);

    /**
     * @brief 生成Redis键名
     * @param prefix 键前缀
     * @param user_id 用户ID
     * @return 完整的Redis键名
     *
     * @details 生成格式为"prefix:user_id"的Redis键名。
     */
    std::string generate_redis_key(const std::string& prefix, const std::string& user_id) const;

    /**
     * @brief 生成设备字段名
     * @param device_id 设备ID
     * @param platform 平台标识
     * @return 设备字段名
     *
     * @details 生成格式为"device_id:platform"的设备字段名。
     */
    std::string generate_device_field(const std::string& device_id,
                                      const std::string& platform) const;

private:
    mutable std::mutex mutex_;                                  ///< 线程安全互斥锁
    std::unique_ptr<PlatformTokenStrategy> platform_strategy_;  ///< 平台令牌策略管理器
    network::WebSocketServer* websocket_server_;                ///< WebSocket服务器指针
};

}  // namespace gateway
}  // namespace im

#endif  // CONNECTION_MANAGER_HPP