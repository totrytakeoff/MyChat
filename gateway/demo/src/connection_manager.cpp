#include "connection_manager.hpp"
#include "common/utils/json.hpp"
#include <algorithm>
#include <sstream>
#include <thread>

using json = nlohmann::json;

// ==================== GatewayWebSocketSession 实现 ====================

GatewayWebSocketSession::GatewayWebSocketSession(std::shared_ptr<WebSocketSession> ws_session)
    : ws_session_(ws_session) {
    if (auto session = ws_session_.lock()) {
        session_id_ = "ws_" + std::to_string(reinterpret_cast<uintptr_t>(session.get())) +
                     "_" + std::to_string(connect_time_.time_since_epoch().count());
    }
}

void GatewayWebSocketSession::SendMessage(const std::string& data) {
    if (auto session = ws_session_.lock()) {
        try {
            session->WriteMsg(data);
            IncrementMessageCount();
        } catch (const std::exception& e) {
            // 发送失败，连接可能已断开
        }
    }
}

void GatewayWebSocketSession::Close() {
    if (auto session = ws_session_.lock()) {
        session->Close();
    }
}

std::string GatewayWebSocketSession::GetSessionId() const {
    return session_id_;
}

std::string GatewayWebSocketSession::GetRemoteAddress() const {
    if (auto session = ws_session_.lock()) {
        // 这里需要根据你的WebSocketSession实现来获取远程地址
        return "websocket_client"; // 简化实现
    }
    return "disconnected";
}

bool GatewayWebSocketSession::IsConnected() const {
    if (auto session = ws_session_.lock()) {
        // 检查WebSocket连接是否还有效
        return true; // 简化实现，实际应该检查连接状态
    }
    return false;
}

// ==================== ConnectionManager 实现 ====================

ConnectionManager::ConnectionManager() : cleanup_running_(false) {
    log_mgr_ = std::make_shared<LogManager>();
    log_mgr_->Init("logs/connection_manager.log");
    log_mgr_->Info("ConnectionManager initialized");
}

ConnectionManager::~ConnectionManager() {
    StopCleanupTask();
    
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
    user_session_map_.clear();
    
    log_mgr_->Info("ConnectionManager destroyed");
}

bool ConnectionManager::AddSession(std::shared_ptr<IGatewaySession> session) {
    if (!session) {
        log_mgr_->Error("AddSession failed: null session");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string session_id = session->GetSessionId();
    if (sessions_.find(session_id) != sessions_.end()) {
        log_mgr_->Warn("Session already exists: " + session_id);
        return false;
    }
    
    sessions_[session_id] = session;
    
    log_mgr_->Info("Added session: " + session_id + 
                  ", total sessions: " + std::to_string(sessions_.size()));
    return true;
}

void ConnectionManager::RemoveSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return;
    }
    
    // 如果会话绑定了用户，需要解绑
    auto session = session_it->second;
    if (session && session->IsAuthenticated()) {
        std::string user_id = session->GetUserId();
        if (!user_id.empty()) {
            auto user_it = user_session_map_.find(user_id);
            if (user_it != user_session_map_.end() && user_it->second == session_id) {
                user_session_map_.erase(user_it);
                log_mgr_->Info("Unbound user: " + user_id + " from session: " + session_id);
            }
        }
    }
    
    sessions_.erase(session_it);
    
    log_mgr_->Info("Removed session: " + session_id + 
                  ", total sessions: " + std::to_string(sessions_.size()));
}

std::shared_ptr<IGatewaySession> ConnectionManager::FindSessionById(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool ConnectionManager::BindUserToSession(const std::string& user_id, const std::string& session_id) {
    if (user_id.empty() || session_id.empty()) {
        log_mgr_->Error("BindUserToSession failed: empty user_id or session_id");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查会话是否存在
    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        log_mgr_->Error("BindUserToSession failed: session not found: " + session_id);
        return false;
    }
    
    auto session = session_it->second;
    if (!session) {
        log_mgr_->Error("BindUserToSession failed: null session: " + session_id);
        return false;
    }
    
    // 检查用户是否已经绑定到其他会话
    auto user_it = user_session_map_.find(user_id);
    if (user_it != user_session_map_.end() && user_it->second != session_id) {
        // 用户在其他会话登录，踢掉之前的会话
        std::string old_session_id = user_it->second;
        auto old_session = FindSessionById(old_session_id);
        if (old_session) {
            old_session->SetAuthenticated(false);
            old_session->SetUserId("");
            log_mgr_->Info("Kicked old session for user: " + user_id);
        }
    }
    
    // 绑定用户到新会话
    user_session_map_[user_id] = session_id;
    session->SetUserId(user_id);
    session->SetAuthenticated(true);
    
    log_mgr_->Info("Bound user: " + user_id + " to session: " + session_id);
    return true;
}

void ConnectionManager::UnbindUser(const std::string& user_id) {
    if (user_id.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto user_it = user_session_map_.find(user_id);
    if (user_it == user_session_map_.end()) {
        return;
    }
    
    std::string session_id = user_it->second;
    auto session = FindSessionById(session_id);
    if (session) {
        session->SetAuthenticated(false);
        session->SetUserId("");
    }
    
    user_session_map_.erase(user_it);
    
    log_mgr_->Info("Unbound user: " + user_id);
}

std::shared_ptr<IGatewaySession> ConnectionManager::FindSessionByUser(const std::string& user_id) {
    if (user_id.empty()) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto user_it = user_session_map_.find(user_id);
    if (user_it == user_session_map_.end()) {
        return nullptr;
    }
    
    return FindSessionById(user_it->second);
}

bool ConnectionManager::IsUserOnline(const std::string& user_id) {
    auto session = FindSessionByUser(user_id);
    return session && session->IsConnected() && session->IsAuthenticated();
}

bool ConnectionManager::PushMessageToUser(const std::string& user_id, const std::string& message) {
    auto session = FindSessionByUser(user_id);
    if (!session || !session->IsConnected()) {
        log_mgr_->Warn("PushMessageToUser failed: user not online: " + user_id);
        return false;
    }
    
    try {
        session->SendMessage(message);
        log_mgr_->Info("Pushed message to user: " + user_id);
        return true;
    } catch (const std::exception& e) {
        log_mgr_->Error("PushMessageToUser failed for user: " + user_id + 
                       ", error: " + e.what());
        return false;
    }
}

size_t ConnectionManager::PushMessageToUsers(const std::vector<std::string>& user_ids, 
                                           const std::string& message) {
    size_t success_count = 0;
    
    for (const auto& user_id : user_ids) {
        if (PushMessageToUser(user_id, message)) {
            ++success_count;
        }
    }
    
    log_mgr_->Info("Pushed message to " + std::to_string(success_count) + 
                  "/" + std::to_string(user_ids.size()) + " users");
    
    return success_count;
}

size_t ConnectionManager::BroadcastMessage(const std::string& message) {
    std::vector<std::shared_ptr<IGatewaySession>> sessions_copy;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_copy.reserve(sessions_.size());
        for (const auto& [session_id, session] : sessions_) {
            if (session && session->IsConnected() && session->IsAuthenticated()) {
                sessions_copy.push_back(session);
            }
        }
    }
    
    size_t success_count = 0;
    for (auto& session : sessions_copy) {
        try {
            session->SendMessage(message);
            ++success_count;
        } catch (const std::exception& e) {
            log_mgr_->Error("Broadcast failed for session: " + session->GetSessionId() +
                           ", error: " + e.what());
        }
    }
    
    log_mgr_->Info("Broadcasted message to " + std::to_string(success_count) + 
                  " sessions");
    
    return success_count;
}

size_t ConnectionManager::GetTotalSessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

size_t ConnectionManager::GetAuthenticatedSessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& [session_id, session] : sessions_) {
        if (session && session->IsAuthenticated()) {
            ++count;
        }
    }
    
    return count;
}

size_t ConnectionManager::GetOnlineUserCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return user_session_map_.size();
}

std::vector<std::string> ConnectionManager::GetOnlineUserList() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> users;
    users.reserve(user_session_map_.size());
    
    for (const auto& [user_id, session_id] : user_session_map_) {
        users.push_back(user_id);
    }
    
    return users;
}

std::string ConnectionManager::GetConnectionStats() const {
    return GenerateSessionStats();
}

size_t ConnectionManager::CleanupInvalidSessions() {
    std::vector<std::string> invalid_sessions;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [session_id, session] : sessions_) {
            if (!session || !session->IsConnected()) {
                invalid_sessions.push_back(session_id);
            }
        }
    }
    
    // 在锁外部移除无效会话
    for (const auto& session_id : invalid_sessions) {
        RemoveSession(session_id);
    }
    
    if (!invalid_sessions.empty()) {
        log_mgr_->Info("Cleaned up " + std::to_string(invalid_sessions.size()) + 
                      " invalid sessions");
    }
    
    return invalid_sessions.size();
}

size_t ConnectionManager::CleanupTimeoutSessions(int timeout_seconds) {
    auto timeout_duration = std::chrono::seconds(timeout_seconds);
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> timeout_sessions;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [session_id, session] : sessions_) {
            if (session && (now - session->GetLastHeartbeat()) > timeout_duration) {
                timeout_sessions.push_back(session_id);
            }
        }
    }
    
    // 在锁外部移除超时会话
    for (const auto& session_id : timeout_sessions) {
        RemoveSession(session_id);
    }
    
    if (!timeout_sessions.empty()) {
        log_mgr_->Info("Cleaned up " + std::to_string(timeout_sessions.size()) + 
                      " timeout sessions");
    }
    
    return timeout_sessions.size();
}

void ConnectionManager::StartCleanupTask() {
    if (cleanup_running_.exchange(true)) {
        return; // 已经在运行
    }
    
    cleanup_thread_ = std::thread([this]() {
        log_mgr_->Info("Cleanup task started");
        
        while (cleanup_running_) {
            try {
                DoCleanup();
            } catch (const std::exception& e) {
                log_mgr_->Error("Cleanup task error: " + std::string(e.what()));
            }
            
            // 每30秒清理一次
            for (int i = 0; i < 30 && cleanup_running_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        log_mgr_->Info("Cleanup task stopped");
    });
}

void ConnectionManager::StopCleanupTask() {
    if (!cleanup_running_.exchange(false)) {
        return; // 已经停止
    }
    
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

void ConnectionManager::DoCleanup() {
    size_t invalid_count = CleanupInvalidSessions();
    size_t timeout_count = CleanupTimeoutSessions(300); // 5分钟超时
    
    if (invalid_count > 0 || timeout_count > 0) {
        log_mgr_->Info("Cleanup completed: " + std::to_string(invalid_count) +
                      " invalid, " + std::to_string(timeout_count) + " timeout");
    }
    
    // 定期输出统计信息
    static int stats_counter = 0;
    if (++stats_counter >= 10) { // 每5分钟输出一次统计信息
        stats_counter = 0;
        log_mgr_->Info("Connection stats: " + GenerateSessionStats());
    }
}

std::string ConnectionManager::GenerateSessionStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["total_sessions"] = sessions_.size();
    stats["authenticated_sessions"] = GetAuthenticatedSessions();
    stats["online_users"] = user_session_map_.size();
    stats["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // 会话类型统计
    size_t websocket_count = 0;
    for (const auto& [session_id, session] : sessions_) {
        if (session_id.find("ws_") == 0) {
            ++websocket_count;
        }
    }
    
    stats["websocket_sessions"] = websocket_count;
    
    return stats.dump();
}