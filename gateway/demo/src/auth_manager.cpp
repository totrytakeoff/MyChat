#include "auth_manager.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <openssl/sha.h>
#include <openssl/hmac.h>

AuthManager::AuthManager() 
    : token_expire_seconds_(86400)
    , max_tokens_per_user_(5)
    , auto_refresh_(false)
    , cleanup_running_(false)
    , total_generated_tokens_(0)
    , total_verify_requests_(0)
    , successful_verifications_(0) {
    
    log_mgr_ = std::make_shared<LogManager>();
    log_mgr_->Init("logs/auth_manager.log");
    log_mgr_->Info("AuthManager initialized");
}

AuthManager::~AuthManager() {
    StopCleanupTask();
    log_mgr_->Info("AuthManager destroyed");
}

bool AuthManager::Initialize(const std::string& secret_key, int expire_seconds) {
    if (secret_key.empty()) {
        log_mgr_->Error("Initialize failed: empty secret key");
        return false;
    }
    
    secret_key_ = secret_key;
    token_expire_seconds_ = expire_seconds;
    
    log_mgr_->Info("AuthManager initialized with expire_seconds: " + 
                  std::to_string(expire_seconds));
    
    // 启动清理任务
    StartCleanupTask();
    
    return true;
}

std::string AuthManager::GenerateToken(const std::string& user_id, 
                                      const std::string& username,
                                      const std::string& device_id) {
    if (user_id.empty() || username.empty()) {
        log_mgr_->Error("GenerateToken failed: empty user_id or username");
        return "";
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查用户Token数量限制
    auto kicked_tokens = CheckAndEvictOldTokens(user_id);
    
    // 生成新Token
    std::string token = GenerateTokenString(user_id);
    if (token.empty()) {
        log_mgr_->Error("Failed to generate token string for user: " + user_id);
        return "";
    }
    
    // 创建用户Token信息
    UserTokenInfo token_info(user_id, username, device_id, token_expire_seconds_);
    
    // 存储Token信息
    token_cache_[token] = token_info;
    user_tokens_map_[user_id].insert(token);
    
    ++total_generated_tokens_;
    
    log_mgr_->Info("Generated token for user: " + user_id + 
                  ", device: " + device_id +
                  ", kicked old tokens: " + std::to_string(kicked_tokens.size()));
    
    return token;
}

bool AuthManager::VerifyToken(const std::string& token, std::string& user_id) {
    UserTokenInfo user_info;
    bool result = VerifyTokenAndGetInfo(token, user_info);
    if (result) {
        user_id = user_info.user_id;
    }
    return result;
}

bool AuthManager::VerifyTokenAndGetInfo(const std::string& token, UserTokenInfo& user_info) {
    if (token.empty()) {
        return false;
    }
    
    ++total_verify_requests_;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 查找Token信息
    auto it = token_cache_.find(token);
    if (it == token_cache_.end()) {
        log_mgr_->Warn("VerifyToken failed: token not found");
        return false;
    }
    
    // 检查Token是否过期
    if (it->second.IsExpired()) {
        log_mgr_->Warn("VerifyToken failed: token expired for user: " + it->second.user_id);
        
        // 清理过期Token
        std::string user_id = it->second.user_id;
        user_tokens_map_[user_id].erase(token);
        if (user_tokens_map_[user_id].empty()) {
            user_tokens_map_.erase(user_id);
        }
        token_cache_.erase(it);
        
        return false;
    }
    
    // 更新最后访问时间
    it->second.UpdateAccess();
    
    // 自动续期
    if (auto_refresh_) {
        auto now = std::chrono::system_clock::now();
        auto half_expire = std::chrono::seconds(token_expire_seconds_ / 2);
        if ((it->second.expire_time - now) < half_expire) {
            it->second.expire_time = now + std::chrono::seconds(token_expire_seconds_);
            log_mgr_->Info("Auto refreshed token for user: " + it->second.user_id);
        }
    }
    
    user_info = it->second;
    ++successful_verifications_;
    
    return true;
}

std::string AuthManager::RefreshToken(const std::string& token) {
    if (token.empty()) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = token_cache_.find(token);
    if (it == token_cache_.end() || it->second.IsExpired()) {
        log_mgr_->Warn("RefreshToken failed: invalid or expired token");
        return "";
    }
    
    // 生成新Token
    std::string new_token = GenerateTokenString(it->second.user_id);
    if (new_token.empty()) {
        log_mgr_->Error("Failed to generate new token during refresh");
        return "";
    }
    
    // 复制用户信息到新Token
    UserTokenInfo new_token_info = it->second;
    new_token_info.create_time = std::chrono::system_clock::now();
    new_token_info.expire_time = new_token_info.create_time + 
                                std::chrono::seconds(token_expire_seconds_);
    new_token_info.UpdateAccess();
    
    // 更新缓存
    token_cache_[new_token] = new_token_info;
    user_tokens_map_[new_token_info.user_id].insert(new_token);
    
    // 移除旧Token
    user_tokens_map_[new_token_info.user_id].erase(token);
    token_cache_.erase(it);
    
    ++total_generated_tokens_;
    
    log_mgr_->Info("Refreshed token for user: " + new_token_info.user_id);
    
    return new_token;
}

bool AuthManager::RevokeToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = token_cache_.find(token);
    if (it == token_cache_.end()) {
        return false;
    }
    
    std::string user_id = it->second.user_id;
    
    // 从用户Token映射中移除
    auto user_it = user_tokens_map_.find(user_id);
    if (user_it != user_tokens_map_.end()) {
        user_it->second.erase(token);
        if (user_it->second.empty()) {
            user_tokens_map_.erase(user_it);
        }
    }
    
    // 从Token缓存中移除
    token_cache_.erase(it);
    
    log_mgr_->Info("Revoked token for user: " + user_id);
    
    return true;
}

size_t AuthManager::RevokeUserTokens(const std::string& user_id) {
    if (user_id.empty()) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto user_it = user_tokens_map_.find(user_id);
    if (user_it == user_tokens_map_.end()) {
        return 0;
    }
    
    size_t revoked_count = 0;
    
    // 移除所有Token
    for (const std::string& token : user_it->second) {
        token_cache_.erase(token);
        ++revoked_count;
    }
    
    user_tokens_map_.erase(user_it);
    
    log_mgr_->Info("Revoked " + std::to_string(revoked_count) + 
                  " tokens for user: " + user_id);
    
    return revoked_count;
}

bool AuthManager::IsUserLoggedIn(const std::string& user_id) {
    if (user_id.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto user_it = user_tokens_map_.find(user_id);
    return user_it != user_tokens_map_.end() && !user_it->second.empty();
}

size_t AuthManager::GetUserTokenCount(const std::string& user_id) {
    if (user_id.empty()) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto user_it = user_tokens_map_.find(user_id);
    if (user_it == user_tokens_map_.end()) {
        return 0;
    }
    
    return user_it->second.size();
}

std::vector<std::string> AuthManager::GetUserDevices(const std::string& user_id) {
    if (user_id.empty()) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> devices;
    
    auto user_it = user_tokens_map_.find(user_id);
    if (user_it == user_tokens_map_.end()) {
        return devices;
    }
    
    for (const std::string& token : user_it->second) {
        auto token_it = token_cache_.find(token);
        if (token_it != token_cache_.end() && !token_it->second.device_id.empty()) {
            devices.push_back(token_it->second.device_id);
        }
    }
    
    // 去重
    std::sort(devices.begin(), devices.end());
    devices.erase(std::unique(devices.begin(), devices.end()), devices.end());
    
    return devices;
}

std::vector<std::string> AuthManager::CheckAndEvictOldTokens(const std::string& user_id) {
    std::vector<std::string> evicted_tokens;
    
    auto user_it = user_tokens_map_.find(user_id);
    if (user_it == user_tokens_map_.end() || user_it->second.size() < max_tokens_per_user_) {
        return evicted_tokens;
    }
    
    // 收集Token及其创建时间
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> token_times;
    
    for (const std::string& token : user_it->second) {
        auto token_it = token_cache_.find(token);
        if (token_it != token_cache_.end()) {
            token_times.emplace_back(token, token_it->second.create_time);
        }
    }
    
    // 按创建时间排序，最老的在前面
    std::sort(token_times.begin(), token_times.end(),
              [](const auto& a, const auto& b) {
                  return a.second < b.second;
              });
    
    // 移除最老的Token，保留最新的几个
    size_t tokens_to_remove = token_times.size() - max_tokens_per_user_ + 1;
    
    for (size_t i = 0; i < tokens_to_remove && i < token_times.size(); ++i) {
        const std::string& token = token_times[i].first;
        
        user_it->second.erase(token);
        token_cache_.erase(token);
        evicted_tokens.push_back(token);
    }
    
    if (!evicted_tokens.empty()) {
        log_mgr_->Info("Evicted " + std::to_string(evicted_tokens.size()) +
                      " old tokens for user: " + user_id);
    }
    
    return evicted_tokens;
}

size_t AuthManager::GetActiveTokenCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return token_cache_.size();
}

size_t AuthManager::GetLoggedInUserCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return user_tokens_map_.size();
}

std::string AuthManager::GetAuthStats() const {
    return GenerateAuthStats();
}

size_t AuthManager::CleanupExpiredTokens() {
    std::vector<std::string> expired_tokens;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 查找过期Token
        for (const auto& [token, token_info] : token_cache_) {
            if (token_info.IsExpired()) {
                expired_tokens.push_back(token);
            }
        }
    }
    
    // 在锁外部移除过期Token
    for (const std::string& token : expired_tokens) {
        RevokeToken(token);
    }
    
    if (!expired_tokens.empty()) {
        log_mgr_->Info("Cleaned up " + std::to_string(expired_tokens.size()) + 
                      " expired tokens");
    }
    
    return expired_tokens.size();
}

void AuthManager::StartCleanupTask() {
    if (cleanup_running_.exchange(true)) {
        return; // 已经在运行
    }
    
    cleanup_thread_ = std::thread([this]() {
        log_mgr_->Info("Auth cleanup task started");
        
        while (cleanup_running_) {
            try {
                DoCleanup();
            } catch (const std::exception& e) {
                log_mgr_->Error("Auth cleanup task error: " + std::string(e.what()));
            }
            
            // 每60秒清理一次
            for (int i = 0; i < 60 && cleanup_running_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        log_mgr_->Info("Auth cleanup task stopped");
    });
}

void AuthManager::StopCleanupTask() {
    if (!cleanup_running_.exchange(false)) {
        return; // 已经停止
    }
    
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

void AuthManager::DoCleanup() {
    size_t expired_count = CleanupExpiredTokens();
    
    if (expired_count > 0) {
        log_mgr_->Info("Auth cleanup completed: " + std::to_string(expired_count) + 
                      " expired tokens removed");
    }
    
    // 定期输出统计信息
    static int stats_counter = 0;
    if (++stats_counter >= 10) { // 每10分钟输出一次统计信息
        stats_counter = 0;
        log_mgr_->Info("Auth stats: " + GenerateAuthStats());
    }
}

std::string AuthManager::GenerateTokenString(const std::string& user_id) {
    // 简化的Token生成算法
    // 实际生产环境建议使用JWT或更安全的算法
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // 创建随机前缀
    std::stringstream ss;
    ss << "tk_";
    
    // 添加时间戳
    ss << std::hex << timestamp;
    
    // 添加随机字符
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    // 添加用户ID的哈希
    std::hash<std::string> hasher;
    size_t user_hash = hasher(user_id + secret_key_);
    ss << "_" << std::hex << (user_hash & 0xFFFFFFFF);
    
    return ss.str();
}

bool AuthManager::ValidateTokenFormat(const std::string& token) {
    // 简单的格式验证
    if (token.empty() || token.length() < 10) {
        return false;
    }
    
    if (token.substr(0, 3) != "tk_") {
        return false;
    }
    
    return true;
}

std::string AuthManager::GenerateAuthStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["active_tokens"] = token_cache_.size();
    stats["logged_in_users"] = user_tokens_map_.size();
    stats["total_generated_tokens"] = total_generated_tokens_.load();
    stats["total_verify_requests"] = total_verify_requests_.load();
    stats["successful_verifications"] = successful_verifications_.load();
    
    if (total_verify_requests_ > 0) {
        stats["success_rate"] = static_cast<double>(successful_verifications_) / 
                               total_verify_requests_;
    } else {
        stats["success_rate"] = 0.0;
    }
    
    stats["config"]["expire_seconds"] = token_expire_seconds_;
    stats["config"]["max_tokens_per_user"] = max_tokens_per_user_;
    stats["config"]["auto_refresh"] = auto_refresh_;
    
    stats["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return stats.dump();
}