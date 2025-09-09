#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

class MinimalGatewayServer {
public:
    MinimalGatewayServer() : http_server_(std::make_unique<httplib::Server>()) {
        setup_routes();
    }
    
    void start() {
        std::cout << "🚀 Starting Minimal Gateway Server on port 8081..." << std::endl;
        
        // HTTP server in separate thread
        server_thread_ = std::thread([this]() {
            http_server_->listen("localhost", 8081);
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "✅ Minimal Gateway Server started" << std::endl;
    }
    
    void stop() {
        std::cout << "🛑 Stopping Minimal Gateway Server..." << std::endl;
        http_server_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        std::cout << "✅ Minimal Gateway Server stopped" << std::endl;
    }
    
private:
    void setup_routes() {
        // Mock login endpoint
        http_server_->Post("/api/v1/auth/login", [](const httplib::Request& req, httplib::Response& res) {
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
                        {"device_id", request_body.value("device_id", "device_001")},
                        {"platform", request_body.value("platform", "test")}
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
        });
        
        // Mock refresh token endpoint
        http_server_->Post("/api/v1/auth/refresh", [](const httplib::Request& req, httplib::Response& res) {
            nlohmann::json response;
            response["status_code"] = 200;
            response["message"] = "Token refreshed";
            response["data"] = {
                {"access_token", "mock_new_access_token_54321"},
                {"refresh_token", "mock_new_refresh_token_09876"}
            };
            res.set_content(response.dump(), "application/json");
        });
        
        // Mock logout endpoint  
        http_server_->Post("/api/v1/auth/logout", [](const httplib::Request& req, httplib::Response& res) {
            nlohmann::json response;
            response["status_code"] = 200;
            response["message"] = "Logout successful";
            res.set_content(response.dump(), "application/json");
        });
        
        // Health check
        http_server_->Get("/health", [](const httplib::Request& req, httplib::Response& res) {
            nlohmann::json response;
            response["status"] = "healthy";
            response["service"] = "minimal_gateway";
            response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            res.set_content(response.dump(), "application/json");
        });

        // Message send endpoint
        http_server_->Post("/api/v1/message/send", [](const httplib::Request& req, httplib::Response& res) {
            nlohmann::json response;
            response["status_code"] = 200;
            response["message"] = "Message sent successfully";
            response["data"] = {
                {"message_id", "msg_12345"},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            res.set_content(response.dump(), "application/json");
        });

        // Message history endpoint
        http_server_->Get("/api/v1/message/history", [](const httplib::Request& req, httplib::Response& res) {
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
        });
    }
    
    std::unique_ptr<httplib::Server> http_server_;
    std::thread server_thread_;
};

// Global server instance for signal handling
MinimalGatewayServer* g_server = nullptr;

void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main() {
    MinimalGatewayServer server;
    g_server = &server;
    
    // Signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    server.start();
    
    // Keep running
    std::cout << "🔄 Press Ctrl+C to stop the server" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}