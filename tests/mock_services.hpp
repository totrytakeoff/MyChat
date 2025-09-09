#ifndef MOCK_SERVICES_HPP
#define MOCK_SERVICES_HPP

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <httplib.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

/**
 * @brief 模拟微服务，用于测试gateway_server
 */
class MockMicroService {
public:
    MockMicroService(int port, const std::string& service_name);
    ~MockMicroService();
    
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // 注册路由处理器
    void register_handler(const std::string& path, 
                         std::function<void(const httplib::Request&, httplib::Response&)> handler);
    
    // 预设的标准响应
    void setup_auth_endpoints();
    void setup_message_endpoints();
    
private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    int port_;
    std::string service_name_;
    
    // 标准认证响应
    void handle_login(const httplib::Request& req, httplib::Response& res);
    void handle_refresh_token(const httplib::Request& req, httplib::Response& res);
    void handle_logout(const httplib::Request& req, httplib::Response& res);
    
    // 标准消息响应
    void handle_send_message(const httplib::Request& req, httplib::Response& res);
    void handle_message_history(const httplib::Request& req, httplib::Response& res);
};

/**
 * @brief Redis模拟服务（用于本地测试，无需真实Redis）
 */
class MockRedisService {
public:
    static MockRedisService& getInstance();
    
    // 模拟Redis操作
    bool set(const std::string& key, const std::string& value, int ttl_seconds = -1);
    bool get(const std::string& key, std::string& value);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    
    // 模拟Hash操作
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    bool hget(const std::string& key, const std::string& field, std::string& value);
    bool hdel(const std::string& key, const std::string& field);
    
    void clear();
    
private:
    MockRedisService() = default;
    std::unordered_map<std::string, std::string> data_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> ttl_;
    std::mutex mutex_;
    
    void cleanup_expired();
};

/**
 * @brief 测试环境设置类
 */
class TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override;
    void TearDown() override;
    
    MockMicroService* getUserService() { return user_service_.get(); }
    MockMicroService* getMessageService() { return message_service_.get(); }
    
private:
    std::unique_ptr<MockMicroService> user_service_;
    std::unique_ptr<MockMicroService> message_service_;
};

// 全局测试环境实例
extern TestEnvironment* g_test_env;

#endif // MOCK_SERVICES_HPP