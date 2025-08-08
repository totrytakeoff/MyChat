#ifndef CONN_MGR_HPP
#define CONN_MGR_HPP

/******************************************************************************
 *
 * @file       conn_mgr.hpp
 * @brief      连接管理器 负责管理所有客户端连接
 *
 * @author     myself
 * @date       2025/08/07
 *
 *****************************************************************************/


#include "../../common/network/websocket_server.hpp"
#include "../../common/network/websocket_session.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace im::gateway {

using im::network::WebSocketServer;
using im::network::WebSocketSession;
using SessionPtr = std::shared_ptr<WebSocketSession>;
using ServerPtr = std::shared_ptr<WebSocketServer>;

class ConnectionManager {
public:

    ConnectionManager(ServerPtr server)
        : server_(std::move(server)) {};
    ~ConnectionManager() = default;

    void set_server(ServerPtr server) { server_ = std::move(server); }

    void bind_user(const std::string& user_id, const std::string& session_id);

    void unbind_user(const std::string& user_id);

    void unbind_session(const std::string& session_id);

    std::string get_session_id(const std::string& user_id) const;

    std::string get_user_id(const std::string& session_id) const;

    std::vector<std::string> get_all_online_users() const;

    SessionPtr get_session(const std::string& session_id) const;

    SessionPtr get_session(const std::string& user_id) const;

    bool is_user_online(const std::string& user_id) const{
        std::lock_guard<std::mutex> lock(map_mutex_);
        return user_to_session_.find(user_id) != user_to_session_.end();
    }

    bool send_to_user(const std::string& user_id, const std::string& message) const;

    size_t get_online_user_count() const {
        std::lock_guard<std::mutex> lock(map_mutex_);
        return user_to_session_.size();
    }
private:
    std::shared_ptr<WebSocketServer> server_;
    mutable std::mutex map_mutex_;
    std::unordered_map<std::string, std::string>
            user_to_session_;  // key : user_id ; value : session_id ;
};


}  // namespace im::gateway
#endif  // CONN_MGR_HPP