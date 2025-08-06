#ifndef CONNECTION_MANAGER_HPP
#define CONNECTION_MANAGER_HPP

/******************************************************************************
 *
 * @file       connection_manager.hpp
 * @brief      连接管理器，负责管理客户端连接
 *
 * @author     MyChat Development Team
 * @date       2025/08/06
 *
 *****************************************************************************/

#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <memory>

#include "common/network/websocket_session.hpp"

namespace im {
namespace gateway {

using SessionPtr = network::SessionPtr;

class ConnectionManager {
public:
    /**
     * @brief 添加连接
     * @param user_id 用户ID
     * @param session WebSocket会话
     */
    void add_connection(const std::string& user_id, SessionPtr session);
    
    /**
     * @brief 移除连接（通过用户ID）
     * @param user_id 用户ID
     */
    void remove_connection(const std::string& user_id);
    
    /**
     * @brief 移除连接（通过会话）
     * @param session WebSocket会话
     */
    void remove_connection(SessionPtr session);
    
    /**
     * @brief 获取会话
     * @param user_id 用户ID
     * @return WebSocket会话
     */
    SessionPtr get_session(const std::string& user_id);
    
    /**
     * @brief 获取在线用户列表
     * @return 在线用户ID列表
     */
    std::vector<std::string> get_online_users();
    
    /**
     * @brief 获取在线用户数量
     * @return 在线用户数量
     */
    size_t get_online_count() const;
    
private:
    std::unordered_map<std::string, SessionPtr> user_sessions_;  // user_id -> session
    std::unordered_map<std::string, std::string> session_users_; // session_id -> user_id
    mutable std::mutex mutex_;
};

} // namespace gateway
} // namespace im

#endif // CONNECTION_MANAGER_HPP