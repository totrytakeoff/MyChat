#ifndef MESSAGE_PROCESSOR_HPP
#define MESSAGE_PROCESSOR_HPP
/******************************************************************************
 *
 * @file       message_processor.hpp
 * @brief      消息类型处理器
 *
 * @author     myself
 * @date       2025/08/20
 *
 *****************************************************************************/

#include <chrono>
#include <memory>
#include <string>

#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/utils/log_manager.hpp"

#include <httplib.h>

namespace im {
namespace gateway {


/**
 * @class      InternalMessage
 * @brief      统一内部消息格式
 *
 */

class InternalMessage {
public:
    enum class Protocol { HTTP, WEBSOCKET, ENUM_END };

    struct MessageContext {
        Protocol protocol;
        std::string session_id;
        std::string client_ip;
        std::chrono::system_clock::time_point timestamp;
    };

    // 构造函数
    InternalMessage() = default;
    InternalMessage(const InternalMessage&) = delete;
    InternalMessage& operator=(const InternalMessage&) = delete;
    InternalMessage(InternalMessage&&) = default;
    InternalMessage& operator=(InternalMessage&&) = default;

    // 消息元数据
    uint32_t get_cmd_id() const;
    const std::string& get_from_uid() const;
    const std::string& get_to_uid() const;
    const std::string& get_token() const;
    const std::string& get_device_id() const;
    const std::string& get_platform() const;

    // 消息体
    const google::protobuf::Message* get_message() const;
    const std::string& get_json_body() const;

    // 上下文信息
    const MessageContext& get_context() const;

    // Setter方法（供MessageFactory使用）
    void set_header(const im::base::IMHeader& header);
    void set_header(im::base::IMHeader&& header);
    void set_protobuf_message(std::unique_ptr<google::protobuf::Message> message);
    void set_json_body(const std::string& json_body);
    void set_json_body(std::string&& json_body);
    void set_context(const MessageContext& context);
    void set_context(MessageContext&& context);

private:
    // 统一的消息头信息
    im::base::IMHeader header_;

    // 消息体（两种格式二选一）
    std::unique_ptr<google::protobuf::Message> protobuf_message_;
    std::string json_body_;

    // 上下文信息
    MessageContext context_;
};

class HttpMessageParser {
public:
    struct HttpParseResult {
        uint32_t cmd_id;
        std::string body_json;
        std::string token;
        std::string device_id;
        std::string platform;
        bool is_valid;
        std::string error_message;
    };
    
    HttpParseResult parse(const httplib::Request& req);
    
    // 从URL路径提取命令ID（供测试使用）
    uint32_t extract_cmd_from_path(const std::string& path);
    
private:
    // 验证HTTP请求格式
    bool validate_http_request(const httplib::Request& req);
    
    // 提取认证信息
    void extract_auth_info(const httplib::Request& req, HttpParseResult& result);
};

class WebSocketMessageParser {
public:
    struct WSParseResult {
        im::base::IMHeader header;
        std::unique_ptr<google::protobuf::Message> message;
        bool is_valid;
        std::string error_message;
    };
    
    WSParseResult parse(const std::string& raw_message);
    
private:
    // 使用现有的ProtobufCodec进行解码
    im::network::ProtobufCodec codec_;
    
    // 验证消息完整性
    bool validate_message(const im::base::IMHeader& header, 
                         const google::protobuf::Message& message);
    
    // 创建对应的消息对象
    std::unique_ptr<google::protobuf::Message> create_message_by_cmd(uint32_t cmd_id);
};

class MessageFactory {
public:
    // 从HTTP解析结果创建InternalMessage
    std::unique_ptr<InternalMessage> create_from_http(
        const HttpMessageParser::HttpParseResult& parse_result,
        const std::string& session_id,
        const std::string& client_ip
    );
    
    // 从WebSocket解析结果创建InternalMessage
    std::unique_ptr<InternalMessage> create_from_websocket(
        const WebSocketMessageParser::WSParseResult& parse_result,
        const std::string& session_id,
        const std::string& client_ip
    );
    
private:
    // 根据命令ID创建对应的Protobuf消息对象
    std::unique_ptr<google::protobuf::Message> create_protobuf_message(uint32_t cmd_id);
    
    // 将JSON转换为Protobuf消息
    bool json_to_protobuf(const std::string& json_str, 
                         google::protobuf::Message* message);
};


class MessageParser {
public:
    // 解析HTTP请求
    std::unique_ptr<InternalMessage> parse_http_request(
        const httplib::Request& req, 
        const std::string& session_id
    );
    
    // 解析WebSocket消息
    std::unique_ptr<InternalMessage> parse_websocket_message(
        const std::string& raw_message,
        const std::string& session_id
    );
    
private:
    std::unique_ptr<HttpMessageParser> http_parser_;
    std::unique_ptr<WebSocketMessageParser> websocket_parser_;
    std::unique_ptr<MessageFactory> message_factory_;
};


}  // namespace gateway
}  // namespace im

#endif  // MESSAGE_PROCESSOR_HPP