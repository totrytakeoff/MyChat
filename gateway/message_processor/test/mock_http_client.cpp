#include "mock_http_client.hpp"
#include <sstream>

namespace im {
namespace test {

httplib::Request MockHttpClient::create_post_request(
    const std::string& path,
    const nlohmann::json& json_body,
    const std::string& token,
    const std::string& device_id,
    const std::string& platform,
    const std::string& client_ip) {
    
    httplib::Request req;
    req.method = "POST";
    req.path = path;
    req.body = json_body.dump();
    
    set_common_headers(req, token, device_id, platform, client_ip);
    req.headers.emplace("Content-Type", "application/json");
    req.headers.emplace("Content-Length", std::to_string(req.body.size()));
    
    return req;
}

httplib::Request MockHttpClient::create_get_request(
    const std::string& path,
    const std::map<std::string, std::string>& params,
    const std::string& token,
    const std::string& device_id,
    const std::string& platform,
    const std::string& client_ip) {
    
    httplib::Request req;
    req.method = "GET";
    req.path = path;
    
    // 添加查询参数
    for (const auto& param : params) {
        req.params.emplace(param.first, param.second);
    }
    
    set_common_headers(req, token, device_id, platform, client_ip);
    
    return req;
}

httplib::Request MockHttpClient::create_login_request(
    const std::string& username,
    const std::string& password) {
    
    nlohmann::json login_body = {
        {"username", username},
        {"password", password},
        {"from_uid", username},
        {"to_uid", "system"}
    };
    
    return create_post_request(
        "/api/v1/user/login",
        login_body,
        "", // 登录时还没有token
        "login_device_001",
        "mobile"
    );
}

httplib::Request MockHttpClient::create_register_request(
    const std::string& username,
    const std::string& password,
    const std::string& email) {
    
    nlohmann::json register_body = {
        {"username", username},
        {"password", password},
        {"email", email},
        {"from_uid", username},
        {"to_uid", "system"}
    };
    
    return create_post_request(
        "/api/v1/user/register",
        register_body,
        "", // 注册时还没有token
        "register_device_002",
        "web"
    );
}

httplib::Request MockHttpClient::create_get_profile_request(
    const std::string& user_id,
    const std::string& token) {
    
    std::map<std::string, std::string> params = {
        {"user_id", user_id},
        {"from_uid", user_id},
        {"to_uid", "system"}
    };
    
    return create_get_request(
        "/api/v1/user/profile",
        params,
        token,
        "profile_device_003",
        "web"
    );
}

httplib::Request MockHttpClient::create_send_message_request(
    const std::string& from_uid,
    const std::string& to_uid,
    const std::string& content,
    const std::string& token) {
    
    nlohmann::json message_body = {
        {"from_uid", from_uid},
        {"to_uid", to_uid},
        {"content", content},
        {"message_type", "text"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    return create_post_request(
        "/api/v1/message/send",
        message_body,
        token,
        "message_device_004",
        "mobile"
    );
}

httplib::Request MockHttpClient::create_get_message_history_request(
    const std::string& user_id,
    const std::string& peer_id,
    int32_t page,
    int32_t page_size,
    const std::string& token) {
    
    std::map<std::string, std::string> params = {
        {"user_id", user_id},
        {"peer_id", peer_id},
        {"page", std::to_string(page)},
        {"page_size", std::to_string(page_size)},
        {"from_uid", user_id},
        {"to_uid", peer_id}
    };
    
    return create_get_request(
        "/api/v1/message/history",
        params,
        token,
        "history_device_005",
        "web"
    );
}

httplib::Request MockHttpClient::create_invalid_request() {
    httplib::Request req;
    req.method = "INVALID_METHOD";
    req.path = "/invalid/path/with spaces and special chars!@#$%";
    req.body = "invalid json {";
    
    return req;
}

httplib::Request MockHttpClient::create_empty_path_request() {
    httplib::Request req;
    req.method = "POST";
    req.path = ""; // 空路径
    req.body = R"({"test": "data"})";
    
    return req;
}

httplib::Request MockHttpClient::create_large_body_request() {
    httplib::Request req;
    req.method = "POST";
    req.path = "/api/v1/test/large";
    
    // 创建一个大的JSON对象
    nlohmann::json large_body;
    large_body["large_data"] = std::string(1024 * 1024, 'A'); // 1MB的数据
    large_body["from_uid"] = "large_user";
    large_body["to_uid"] = "target_user";
    
    req.body = large_body.dump();
    
    set_common_headers(req, "large_token", "large_device", "test", "127.0.0.1");
    req.headers.emplace("Content-Type", "application/json");
    req.headers.emplace("Content-Length", std::to_string(req.body.size()));
    
    return req;
}

void MockHttpClient::set_common_headers(
    httplib::Request& req,
    const std::string& token,
    const std::string& device_id,
    const std::string& platform,
    const std::string& client_ip) {
    
    if (!token.empty()) {
        req.headers.emplace("Authorization", "Bearer " + token);
        req.headers.emplace("X-Token", token);
    }
    
    req.headers.emplace("X-Device-Id", device_id);
    req.headers.emplace("X-Platform", platform);
    req.headers.emplace("X-Real-IP", client_ip);
    req.headers.emplace("X-Forwarded-For", client_ip);
    req.headers.emplace("User-Agent", "MockHttpClient/1.0");
    req.headers.emplace("Accept", "application/json");
}

std::string MockHttpClient::build_query_string(const std::map<std::string, std::string>& params) {
    std::ostringstream oss;
    bool first = true;
    
    for (const auto& param : params) {
        if (!first) {
            oss << "&";
        }
        oss << param.first << "=" << param.second;
        first = false;
    }
    
    return oss.str();
}

} // namespace test
} // namespace im