#ifndef MOCK_WEBSOCKET_CLIENT_HPP
#define MOCK_WEBSOCKET_CLIENT_HPP

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "../../../common/network/protobuf_codec.hpp"
#include "../../../common/proto/base.pb.h"

namespace im {
namespace test {

/**
 * @brief Mock WebSocket客户端，用于测试消息解析器
 */
class MockWebSocketClient {
public:
    MockWebSocketClient();
    ~MockWebSocketClient() = default;

    /**
     * @brief 创建测试用的WebSocket消息
     * @param cmd_id 命令ID
     * @param token 认证令牌
     * @param device_id 设备ID
     * @param platform 平台信息
     * @param from_uid 发送者UID
     * @param to_uid 接收者UID
     * @param message_type 消息类型 (用于BaseResponse的data字段)
     * @return 编码后的二进制消息
     */
    std::string create_websocket_message(
        uint32_t cmd_id,
        const std::string& token = "test_token_123456",
        const std::string& device_id = "test_device_001", 
        const std::string& platform = "test_platform",
        const std::string& from_uid = "user_001",
        const std::string& to_uid = "user_002",
        const std::string& message_type = "ping"
    );

    /**
     * @brief 创建简单的心跳消息
     */
    std::string create_heartbeat_message();

    /**
     * @brief 创建登录消息
     */
    std::string create_login_message(
        const std::string& username,
        const std::string& password,
        const std::string& token = "login_token_123"
    );

    /**
     * @brief 创建聊天消息
     */
    std::string create_chat_message(
        const std::string& from_uid,
        const std::string& to_uid,
        const std::string& content,
        const std::string& token = "chat_token_456"
    );

    /**
     * @brief 创建无效消息（用于测试错误处理）
     */
    std::string create_invalid_message();

    /**
     * @brief 创建空消息
     */
    std::string create_empty_message();

    /**
     * @brief 创建超大消息（用于测试大小限制）
     */
    std::string create_large_message();

private:
    im::network::ProtobufCodec codec_;

    /**
     * @brief 创建基础的IMHeader
     */
    im::base::IMHeader create_base_header(
        uint32_t cmd_id,
        const std::string& token,
        const std::string& device_id,
        const std::string& platform,
        const std::string& from_uid = "",
        const std::string& to_uid = ""
    );

    /**
     * @brief 创建BaseResponse消息
     */
    im::base::BaseResponse create_base_response(
        int32_t code = 0,
        const std::string& message = "success",
        const std::string& data = ""
    );
};

} // namespace test
} // namespace im

#endif // MOCK_WEBSOCKET_CLIENT_HPP