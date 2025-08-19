#ifndef MOCK_OBJECTS_HPP
#define MOCK_OBJECTS_HPP

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

#include "common/network/websocket_session.hpp"
#include "common/network/websocket_server.hpp"
#include "common/database/redis_mgr.hpp"
#include "gateway/auth/multi_platform_auth.hpp"
#include "common/utils/global.hpp"

namespace im {
namespace gateway {
namespace test {

// Simple mock WebSocket Session that can work with ConnectionManager
class MockWebSocketSession {
public:
    MockWebSocketSession() {
        session_id_ = "mock_session_" + std::to_string(session_counter_++);
    }
    virtual ~MockWebSocketSession() = default;

    virtual void start() {}
    virtual void close() {}
    virtual void send(const std::string& message) {}
    
    virtual const std::string& get_session_id() const {
        return session_id_;
    }

private:
    static inline int session_counter_ = 0;
    std::string session_id_;
};

// Mock WebSocket Server
class MockWebSocketServer {
public:
    MOCK_METHOD(void, start, ());
    MOCK_METHOD(void, stop, ());
    MOCK_METHOD(void, add_session, (network::SessionPtr session));
    MOCK_METHOD(void, remove_session, (network::SessionPtr session));
    MOCK_METHOD(void, remove_session, (const std::string& session_id));
    MOCK_METHOD(size_t, get_session_count, (), (const));
    
    network::SessionPtr get_session(const std::string& session_id) const {
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    void add_session_internal(const std::string& session_id, network::SessionPtr session) {
        sessions_[session_id] = session;
    }
    
    void remove_session_internal(const std::string& session_id) {
        sessions_.erase(session_id);
    }

private:
    mutable std::unordered_map<std::string, network::SessionPtr> sessions_;
};

// Mock Redis Manager  
class MockRedisManager {
public:
    MOCK_METHOD(bool, initialize, (const std::string& config_path));
    MOCK_METHOD(bool, initialize, (const db::RedisConfig& config));
    
    // Mock Redis operations
    MOCK_METHOD(void, hset, (const std::string& key, const std::string& field, const std::string& value));
    MOCK_METHOD((std::optional<std::string>), hget, (const std::string& key, const std::string& field));
    MOCK_METHOD(void, hdel, (const std::string& key, const std::string& field));
    MOCK_METHOD((std::unordered_map<std::string, std::string>), hgetall, (const std::string& key));
    MOCK_METHOD(void, sadd, (const std::string& key, const std::string& member));
    MOCK_METHOD(void, srem, (const std::string& key, const std::string& member));
    MOCK_METHOD((std::unordered_set<std::string>), smembers, (const std::string& key));
    MOCK_METHOD(size_t, scard, (const std::string& key));
    MOCK_METHOD(void, del, (const std::string& key));
    
    // Mock execute method
    template<typename Func>
    auto execute(Func&& func) -> decltype(func(*this)) {
        return func(*this);
    }
    
    // Static mock instance
    static MockRedisManager& GetInstance() {
        static MockRedisManager instance;
        return instance;
    }
};

// Mock Platform Token Strategy
class MockPlatformTokenStrategy {
public:
    MOCK_METHOD(PlatformTokenConfig, get_platform_token_config, (const std::string& platform));
    
    // Set up default behavior
    void SetupDefaultConfig() {
        PlatformTokenConfig config;
        config.platform = PlatformType::ANDROID;
        config.enable_multi_device = true;
        config.token_time_config.access_token_expire_seconds = 3600;
        config.token_time_config.refresh_token_expire_seconds = 86400;
        config.refresh_config.auto_refresh_enabled = true;
        config.refresh_config.refresh_precentage = 0.2f;
        config.refresh_config.background_refresh = true;
        config.refresh_config.max_retry_count = 3;
        
        ON_CALL(*this, get_platform_token_config(testing::_))
            .WillByDefault(testing::Return(config));
    }
    
    void SetupSingleDeviceConfig() {
        PlatformTokenConfig config;
        config.platform = PlatformType::IOS;
        config.enable_multi_device = false;
        config.token_time_config.access_token_expire_seconds = 3600;
        config.token_time_config.refresh_token_expire_seconds = 86400;
        config.refresh_config.auto_refresh_enabled = true;
        config.refresh_config.refresh_precentage = 0.2f;
        config.refresh_config.background_refresh = true;
        config.refresh_config.max_retry_count = 3;
        
        ON_CALL(*this, get_platform_token_config(testing::_))
            .WillByDefault(testing::Return(config));
    }
};

}  // namespace test
}  // namespace gateway
}  // namespace im

#endif  // MOCK_OBJECTS_HPP