#include "mock_websocket_client.hpp"
#include <chrono>
#include <random>

namespace im {
namespace test {

MockWebSocketClient::MockWebSocketClient() {
    // 初始化编解码器
}

std::string MockWebSocketClient::create_websocket_message(
    uint32_t cmd_id,
    const std::string& token,
    const std::string& device_id,
    const std::string& platform,
    const std::string& from_uid,
    const std::string& to_uid,
    const std::string& message_type) {
    
    // 创建消息头
    auto header = create_base_header(cmd_id, token, device_id, platform, from_uid, to_uid);
    
    // 创建消息体
    auto response = create_base_response(0, "success", message_type);
    
    // 编码消息
    std::string encoded_message;
    if (codec_.encode(header, response, encoded_message)) {
        return encoded_message;
    }
    
    return ""; // 编码失败返回空字符串
}

std::string MockWebSocketClient::create_heartbeat_message() {
    return create_websocket_message(
        1001, // 心跳命令ID
        "heartbeat_token",
        "heartbeat_device",
        "test_platform",
        "",
        "",
        "heartbeat"
    );
}

std::string MockWebSocketClient::create_login_message(
    const std::string& username,
    const std::string& password,
    const std::string& token) {
    
    // 创建登录数据
    std::string login_data = R"({
        "username": ")" + username + R"(",
        "password": ")" + password + R"("
    })";
    
    return create_websocket_message(
        10001, // 登录命令ID
        token,
        "login_device_001",
        "mobile",
        username,
        "",
        login_data
    );
}

std::string MockWebSocketClient::create_chat_message(
    const std::string& from_uid,
    const std::string& to_uid,
    const std::string& content,
    const std::string& token) {
    
    // 创建聊天数据
    std::string chat_data = R"({
        "content": ")" + content + R"(",
        "message_type": "text",
        "timestamp": )" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) + R"(
    })";
    
    return create_websocket_message(
        20001, // 发送消息命令ID
        token,
        "chat_device_001",
        "web",
        from_uid,
        to_uid,
        chat_data
    );
}

std::string MockWebSocketClient::create_invalid_message() {
    // 返回无效的二进制数据
    return std::string("\x00\x01\x02\x03\x04", 5);
}

std::string MockWebSocketClient::create_empty_message() {
    return "";
}

std::string MockWebSocketClient::create_large_message() {
    // 创建一个超过10MB的消息
    std::string large_data(11 * 1024 * 1024, 'A'); // 11MB的数据
    
    return create_websocket_message(
        9999,
        "large_token",
        "large_device",
        "test",
        "large_user",
        "target_user",
        large_data
    );
}

im::base::IMHeader MockWebSocketClient::create_base_header(
    uint32_t cmd_id,
    const std::string& token,
    const std::string& device_id,
    const std::string& platform,
    const std::string& from_uid,
    const std::string& to_uid) {
    
    im::base::IMHeader header;
    header.set_version("1.0");
    header.set_seq(static_cast<uint32_t>(std::time(nullptr))); // 使用时间戳作为序列号
    header.set_cmd_id(cmd_id);
    header.set_token(token);
    header.set_device_id(device_id);
    header.set_platform(platform);
    
    if (!from_uid.empty()) {
        header.set_from_uid(from_uid);
    }
    if (!to_uid.empty()) {
        header.set_to_uid(to_uid);
    }
    
    // 设置时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    header.set_timestamp(static_cast<uint64_t>(timestamp_ms));
    
    return header;
}

im::base::BaseResponse MockWebSocketClient::create_base_response(
    int32_t code,
    const std::string& message,
    const std::string& data) {
    
    im::base::BaseResponse response;
    response.set_error_code(static_cast<im::base::ErrorCode>(code));
    response.set_error_message(message);
    response.set_payload(data);
    
    return response;
}

} // namespace test
} // namespace im