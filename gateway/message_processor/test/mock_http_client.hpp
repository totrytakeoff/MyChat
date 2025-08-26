#ifndef MOCK_HTTP_CLIENT_HPP
#define MOCK_HTTP_CLIENT_HPP

#include <string>
#include <map>
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace im {
namespace test {

/**
 * @brief Mock HTTP客户端，用于测试消息解析器
 */
class MockHttpClient {
public:
    MockHttpClient() = default;
    ~MockHttpClient() = default;

    /**
     * @brief 创建POST请求
     * @param path 请求路径
     * @param json_body JSON请求体
     * @param token 认证令牌
     * @param device_id 设备ID
     * @param platform 平台信息
     * @param client_ip 客户端IP
     * @return httplib::Request对象
     */
    httplib::Request create_post_request(
        const std::string& path,
        const nlohmann::json& json_body,
        const std::string& token = "test_token_123456",
        const std::string& device_id = "test_device_001",
        const std::string& platform = "web",
        const std::string& client_ip = "127.0.0.1"
    );

    /**
     * @brief 创建GET请求
     * @param path 请求路径  
     * @param params 查询参数
     * @param token 认证令牌
     * @param device_id 设备ID
     * @param platform 平台信息
     * @param client_ip 客户端IP
     * @return httplib::Request对象
     */
    httplib::Request create_get_request(
        const std::string& path,
        const std::map<std::string, std::string>& params = {},
        const std::string& token = "test_token_123456", 
        const std::string& device_id = "test_device_001",
        const std::string& platform = "web",
        const std::string& client_ip = "127.0.0.1"
    );

    /**
     * @brief 创建用户登录请求
     */
    httplib::Request create_login_request(
        const std::string& username,
        const std::string& password
    );

    /**
     * @brief 创建用户注册请求
     */
    httplib::Request create_register_request(
        const std::string& username,
        const std::string& password,
        const std::string& email
    );

    /**
     * @brief 创建获取用户信息请求
     */
    httplib::Request create_get_profile_request(
        const std::string& user_id,
        const std::string& token = "profile_token_123"
    );

    /**
     * @brief 创建发送消息请求
     */
    httplib::Request create_send_message_request(
        const std::string& from_uid,
        const std::string& to_uid,
        const std::string& content,
        const std::string& token = "message_token_456"
    );

    /**
     * @brief 创建获取消息历史请求
     */
    httplib::Request create_get_message_history_request(
        const std::string& user_id,
        const std::string& peer_id,
        int32_t page = 1,
        int32_t page_size = 20,
        const std::string& token = "history_token_789"
    );

    /**
     * @brief 创建无效的HTTP请求（用于测试错误处理）
     */
    httplib::Request create_invalid_request();

    /**
     * @brief 创建空路径请求
     */
    httplib::Request create_empty_path_request();

    /**
     * @brief 创建超大请求体请求
     */
    httplib::Request create_large_body_request();

private:
    /**
     * @brief 设置通用的HTTP头
     */
    void set_common_headers(
        httplib::Request& req,
        const std::string& token,
        const std::string& device_id,
        const std::string& platform,
        const std::string& client_ip
    );

    /**
     * @brief 构建查询字符串
     */
    std::string build_query_string(const std::map<std::string, std::string>& params);
};

} // namespace test 
} // namespace im

#endif // MOCK_HTTP_CLIENT_HPP