#include "conn_mgr.hpp"

namespace im::gateway {

void ConnectionManager::bind_user(const std::string& user_id, const std::string& session_id) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    user_to_session_[user_id] = session_id;
}

void ConnectionManager::unbind_user(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    user_to_session_.erase(user_id);
}

void ConnectionManager::unbind_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    for (auto it = user_to_session_.begin(); it != user_to_session_.end();) {
        if (it->second == session_id) {
            it = user_to_session_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string ConnectionManager::get_session_id(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(map_mutex_);

    auto it = user_to_session_.find(user_id);
    if (it != user_to_session_.end()) {
        return it->second;
    }
    return "";
}

std::string ConnectionManager::get_user_id(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(map_mutex_);

    for (const auto& pair : user_to_session_) {
        if (pair.second == session_id) {
            return pair.first;
        }
    }
    return "";
}

std::vector<std::string> ConnectionManager::get_all_online_users() const {
    std::lock_guard<std::mutex> lock(map_mutex_);
    std::vector<std::string> online_users;
    for (const auto& pair : user_to_session_) {
        online_users.push_back(pair.first);
    }
    return online_users;
}

SessionPtr ConnectionManager::get_session(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(map_mutex_);
    if (server_) {
        return server_->get_session(session_id);
    }
    return nullptr;
}
SessionPtr ConnectionManager::get_session(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto session_id = get_session_id(user_id);
    if (session_id.empty()) {
        return nullptr;
    }
    if (server_) {
        return server_->get_session(session_id);
    }
    return nullptr;
}

bool ConnectionManager::send_to_user(const std::string& user_id, const std::string& message) const {
    auto session  = get_session(user_id);

    if (session) {
        session->send(message);
        return true;
    }
    return false;
}



}  // namespace im::gateway
