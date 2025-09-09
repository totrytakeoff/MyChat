#include "mock_services.hpp"
#include <iostream>
#include <sstream>

TestEnvironment* g_test_env = nullptr;

// MockMicroService implementation
MockMicroService::MockMicroService(int port, const std::string& service_name)
    : port_(port), service_name_(service_name), running_(false) {
    server_ = std::make_unique<httplib::Server>();
}

MockMicroService::~MockMicroService() {
    stop();
}

void MockMicroService::start() {
    if (running_) return;
    
    running_ = true;
    server_thread_ = std::thread([this]() {
        std::cout << "Starting " << service_name_ << " on port " << port_ << std::endl;
        server_->listen("localhost", port_);
    });
    
    // 等待服务启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void MockMicroService::stop() {
    if (!running_) return;
    
    running_ = false;
    server_->stop();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void MockMicroService::register_handler(const std::string& path, 
                                       std::function<void(const httplib::Request&, httplib::Response&)> handler) {
    server_->Post(path, handler);
}

void MockMicroService::setup_auth_endpoints() {
    // 登录接口
    server_->Post("/auth/login", [this](const httplib::Request& req, httplib::Response& res) {
        handle_login(req, res);
    });
    
    // Token刷新接口
    server_->Post("/auth/refresh", [this](const httplib::Request& req, httplib::Response& res) {
        handle_refresh_token(req, res);
    });
    
    // 登出接口
    server_->Post("/auth/logout", [this](const httplib::Request& req, httplib::Response& res) {
        handle_logout(req, res);
    });
}

void MockMicroService::setup_message_endpoints() {
    // 发送消息接口
    server_->Post("/message/send", [this](const httplib::Request& req, httplib::Response& res) {
        handle_send_message(req, res);
    });
    
    // 消息历史接口
    server_->Get("/message/history", [this](const httplib::Request& req, httplib::Response& res) {
        handle_message_history(req, res);
    });
}

void MockMicroService::handle_login(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response;
    
    try {
        auto request_body = nlohmann::json::parse(req.body);
        std::string username = request_body.value("username", "");
        std::string password = request_body.value("password", "");
        
        if (username == "testuser" && password == "testpass") {
            response["status_code"] = 200;
            response["message"] = "Login successful";
            response["data"] = {
                {"access_token", "mock_access_token_12345"},
                {"refresh_token", "mock_refresh_token_67890"},
                {"user_id", "user_001"},
                {"device_id", "device_001"},
                {"platform", "test"}
            };
        } else {
            response["status_code"] = 401;
            response["message"] = "Invalid credentials";
        }
    } catch (const std::exception& e) {
        response["status_code"] = 400;
        response["message"] = "Invalid request format";
    }
    
    res.set_content(response.dump(), "application/json");
}

void MockMicroService::handle_refresh_token(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response;
    
    try {
        auto request_body = nlohmann::json::parse(req.body);
        std::string refresh_token = request_body.value("refresh_token", "");
        
        if (refresh_token == "mock_refresh_token_67890") {
            response["status_code"] = 200;
            response["message"] = "Token refreshed successfully";
            response["data"] = {
                {"access_token", "mock_new_access_token_54321"},
                {"refresh_token", "mock_new_refresh_token_09876"}
            };
        } else {
            response["status_code"] = 401;
            response["message"] = "Invalid refresh token";
        }
    } catch (const std::exception& e) {
        response["status_code"] = 400;
        response["message"] = "Invalid request format";
    }
    
    res.set_content(response.dump(), "application/json");
}

void MockMicroService::handle_logout(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response;
    response["status_code"] = 200;
    response["message"] = "Logout successful";
    
    res.set_content(response.dump(), "application/json");
}

void MockMicroService::handle_send_message(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response;
    
    try {
        auto request_body = nlohmann::json::parse(req.body);
        
        response["status_code"] = 200;
        response["message"] = "Message sent successfully";
        response["data"] = {
            {"message_id", "msg_12345"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
    } catch (const std::exception& e) {
        response["status_code"] = 400;
        response["message"] = "Invalid message format";
    }
    
    res.set_content(response.dump(), "application/json");
}

void MockMicroService::handle_message_history(const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response;
    
    response["status_code"] = 200;
    response["message"] = "Message history retrieved";
    response["data"] = {
        {"messages", nlohmann::json::array({
            {{"id", "msg_001"}, {"content", "Hello"}, {"sender", "user_001"}},
            {{"id", "msg_002"}, {"content", "Hi there"}, {"sender", "user_002"}}
        })}
    };
    
    res.set_content(response.dump(), "application/json");
}

// MockRedisService implementation
MockRedisService& MockRedisService::getInstance() {
    static MockRedisService instance;
    return instance;
}

bool MockRedisService::set(const std::string& key, const std::string& value, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key] = value;
    
    if (ttl_seconds > 0) {
        ttl_[key] = std::chrono::system_clock::now() + std::chrono::seconds(ttl_seconds);
    }
    
    return true;
}

bool MockRedisService::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup_expired();
    
    auto it = data_.find(key);
    if (it != data_.end()) {
        value = it->second;
        return true;
    }
    return false;
}

bool MockRedisService::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.erase(key);
    ttl_.erase(key);
    return true;
}

bool MockRedisService::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup_expired();
    return data_.find(key) != data_.end();
}

bool MockRedisService::hset(const std::string& key, const std::string& field, const std::string& value) {
    return set(key + ":" + field, value);
}

bool MockRedisService::hget(const std::string& key, const std::string& field, std::string& value) {
    return get(key + ":" + field, value);
}

bool MockRedisService::hdel(const std::string& key, const std::string& field) {
    return del(key + ":" + field);
}

void MockRedisService::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    ttl_.clear();
}

void MockRedisService::cleanup_expired() {
    auto now = std::chrono::system_clock::now();
    auto it = ttl_.begin();
    while (it != ttl_.end()) {
        if (it->second < now) {
            data_.erase(it->first);
            it = ttl_.erase(it);
        } else {
            ++it;
        }
    }
}

// TestEnvironment implementation
void TestEnvironment::SetUp() {
    std::cout << "Setting up test environment..." << std::endl;
    
    // 启动模拟微服务
    user_service_ = std::make_unique<MockMicroService>(9001, "test_user_service");
    message_service_ = std::make_unique<MockMicroService>(9002, "test_message_service");
    
    user_service_->setup_auth_endpoints();
    message_service_->setup_message_endpoints();
    
    user_service_->start();
    message_service_->start();
    
    // 等待服务完全启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "Test environment setup complete" << std::endl;
}

void TestEnvironment::TearDown() {
    std::cout << "Tearing down test environment..." << std::endl;
    
    if (user_service_) {
        user_service_->stop();
    }
    if (message_service_) {
        message_service_->stop();
    }
    
    // 清理模拟Redis
    MockRedisService::getInstance().clear();
    
    std::cout << "Test environment teardown complete" << std::endl;
}