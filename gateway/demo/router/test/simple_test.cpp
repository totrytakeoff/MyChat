#include "../simple_processor.hpp"
#include <iostream>
#include <thread>

using namespace im::gateway;

// 模拟HTTP服务器
class SimpleServer {
public:
    SimpleServer() : processor_() {}
    
    bool start(const std::string& config_file, const std::string& host = "localhost", int port = 8080) {
        // 加载配置
        if (!processor_.load_config(config_file)) {
            std::cerr << "配置加载失败" << std::endl;
            return false;
        }
        
        std::cout << "启动HTTP服务器: " << host << ":" << port << std::endl;
        
        // 设置路由处理
        server_.Post("/.*", [this](const httplib::Request& req, httplib::Response& res) {
            processor_.handle_request(req, res);
        });
        
        server_.Get("/.*", [this](const httplib::Request& req, httplib::Response& res) {
            processor_.handle_request(req, res);
        });
        
        // 启动服务器
        return server_.listen(host.c_str(), port);
    }
    
    void stop() {
        server_.stop();
    }

private:
    httplib::Server server_;
    SimpleProcessor processor_;
};

// 模拟客户端测试
void test_client() {
    std::cout << "\n=== 开始客户端测试 ===" << std::endl;
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    httplib::Client client("localhost", 8080);
    
    // 测试1: 登录请求
    std::cout << "\n--- 测试1: 登录请求 ---" << std::endl;
    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer test_token_123"},
        {"X-Device-ID", "device_456"},
        {"X-Platform", "android"}
    };
    
    std::string login_body = R"({"username": "testuser", "password": "testpass"})";
    auto login_res = client.Post("/api/v1/auth/login", headers, login_body, "application/json");
    
    if (login_res) {
        std::cout << "登录响应状态: " << login_res->status << std::endl;
        std::cout << "登录响应内容: " << login_res->body << std::endl;
    } else {
        std::cout << "登录请求失败" << std::endl;
    }
    
    // 测试2: 获取好友列表
    std::cout << "\n--- 测试2: 获取好友列表 ---" << std::endl;
    auto friends_res = client.Get("/api/v1/friend/list", headers);
    
    if (friends_res) {
        std::cout << "好友列表响应状态: " << friends_res->status << std::endl;
        std::cout << "好友列表响应内容: " << friends_res->body << std::endl;
    } else {
        std::cout << "好友列表请求失败" << std::endl;
    }
    
    // 测试3: 发送消息
    std::cout << "\n--- 测试3: 发送消息 ---" << std::endl;
    std::string message_body = R"({"to_user": "friend1", "content": "Hello World!", "type": "text"})";
    auto message_res = client.Post("/api/v1/message/send", headers, message_body, "application/json");
    
    if (message_res) {
        std::cout << "发送消息响应状态: " << message_res->status << std::endl;
        std::cout << "发送消息响应内容: " << message_res->body << std::endl;
    } else {
        std::cout << "发送消息请求失败" << std::endl;
    }
    
    // 测试4: 未知路由
    std::cout << "\n--- 测试4: 未知路由 ---" << std::endl;
    auto unknown_res = client.Get("/api/v1/unknown/path", headers);
    
    if (unknown_res) {
        std::cout << "未知路由响应状态: " << unknown_res->status << std::endl;
        std::cout << "未知路由响应内容: " << unknown_res->body << std::endl;
    } else {
        std::cout << "未知路由请求失败" << std::endl;
    }
    
    std::cout << "\n=== 客户端测试完成 ===" << std::endl;
}

int main() {
    std::cout << "=== 简单HTTP处理器测试 ===" << std::endl;
    
    // 启动服务器（在后台线程）
    SimpleServer server;
    std::thread server_thread([&server]() {
        server.start("../config/simple_config.json");
    });
    
    // 运行客户端测试
    test_client();
    
    // 停止服务器
    server.stop();
    
    // 等待服务器线程结束
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    std::cout << "\n=== 测试程序结束 ===" << std::endl;
    return 0;
}