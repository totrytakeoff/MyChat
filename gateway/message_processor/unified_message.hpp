#ifndef UNIFIED_MESSAGE_HPP
#define UNIFIED_MESSAGE_HPP


/******************************************************************************
 *
 * @file       unified_message.hpp
 * @brief      通用消息格式
 *
 * @author     myself
 * @date       2025/08/22
 *
 *****************************************************************************/

#include <chrono>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include "../../common/proto/base.pb.h"

namespace im {
namespace gateway {

/**
 * @class UnifiedMessage
 * @brief 精简的统一消息格式
 *
 */
class UnifiedMessage {
public:
    enum class Protocol { HTTP, WEBSOCKET, ENUM_END };

    struct SessionContext {
        Protocol protocol;                                   // 消息来源协议
        std::string session_id;                              // 会话ID
        std::string client_ip;                               // 客户端IP
        std::chrono::system_clock::time_point receive_time;  // 接收时间

        // HTTP专用字段
        std::string http_method;    // HTTP方法 (GET/POST等)
        std::string original_path;  // 原始路径
        std::string raw_body;       // 原始请求体
    };

    // 构造函数
    UnifiedMessage() = default;
    UnifiedMessage(const UnifiedMessage&) = delete;
    UnifiedMessage& operator=(const UnifiedMessage&) = delete;
    UnifiedMessage(UnifiedMessage&&) = default;
    UnifiedMessage& operator=(UnifiedMessage&&) = default;

    // ===== 核心访问接口 =====

    // 从IMHeader获取的信息（最重要的）
    uint32_t get_cmd_id() const { return header_.cmd_id(); }
    const std::string& get_token() const { return header_.token(); }
    const std::string& get_device_id() const { return header_.device_id(); }
    const std::string& get_platform() const { return header_.platform(); }
    const std::string& get_from_uid() const { return header_.from_uid(); }
    const std::string& get_to_uid() const { return header_.to_uid(); }
    uint64_t get_timestamp() const { return header_.timestamp(); }

    // 消息体访问
    const google::protobuf::Message* get_protobuf_message() const {
        return protobuf_message_.get();
    }
    const std::string& get_json_body() const { return json_body_; }

    // 会话信息访问
    const SessionContext& get_session_context() const { return session_context_; }
    Protocol get_protocol() const { return session_context_.protocol; }
    const std::string& get_session_id() const { return session_context_.session_id; }

    // 完整的Header访问（给需要的地方用）
    const im::base::IMHeader& get_header() const { return header_; }

    // ===== 设置接口（给MessageProcessor用） =====

    void set_header(const im::base::IMHeader& header) { header_ = header; }
    void set_header(im::base::IMHeader&& header) { header_ = std::move(header); }
    void set_protobuf_message(std::unique_ptr<google::protobuf::Message> message) {
        protobuf_message_ = std::move(message);
    }
    void set_json_body(const std::string& json_body) { json_body_ = json_body; }
    void set_json_body(std::string&& json_body) { json_body_ = std::move(json_body); }
    
    // 原始Protobuf数据访问器（优化WebSocket处理）
    void set_raw_protobuf_data(const std::string& raw_data) { raw_protobuf_data_ = raw_data; }
    void set_raw_protobuf_data(std::string&& raw_data) { raw_protobuf_data_ = std::move(raw_data); }
    
    void set_session_context(const SessionContext& context) { session_context_ = context; }
    void set_session_context(SessionContext&& context) { session_context_ = std::move(context); }

    // ===== 便利方法 =====

    // 获取器方法
    const std::string& get_raw_protobuf_data() const { return raw_protobuf_data_; }
    
    bool is_http() const { return session_context_.protocol == Protocol::HTTP; }
    bool is_websocket() const { return session_context_.protocol == Protocol::WEBSOCKET; }
    bool has_protobuf_message() const { return protobuf_message_ != nullptr; }
    bool has_json_body() const { return !json_body_.empty(); }
    bool has_raw_protobuf_data() const { return !raw_protobuf_data_.empty(); }

    // 调试打印
    void print_info() const {
        std::ostringstream oss = format_info();
        std::cout << oss.str();
    }
    
    // 格式化信息为字符串流，便于自定义输出目标
    std::ostringstream format_info() const {
        std::ostringstream oss;
        oss << "\n=== 统一消息信息 ===" << std::endl;
        oss << "协议类型: " << (is_http() ? "HTTP" : "WebSocket") << std::endl;
        oss << "命令ID: " << get_cmd_id() << std::endl;
        oss << "会话ID: " << get_session_id() << std::endl;

        if (!get_token().empty()) {
            oss << "Token: " << get_token().substr(0, 10) << "..." << std::endl;
        }
        if (!get_device_id().empty()) {
            oss << "设备ID: " << get_device_id() << std::endl;
        }
        if (!get_platform().empty()) {
            oss << "平台: " << get_platform() << std::endl;
        }
        if (!get_from_uid().empty()) {
            oss << "发送者: " << get_from_uid() << std::endl;
        }
        if (!get_to_uid().empty()) {
            oss << "接收者: " << get_to_uid() << std::endl;
        }

        if (is_http()) {
            oss << "HTTP方法: " << session_context_.http_method << std::endl;
            oss << "原始路径: " << session_context_.original_path << std::endl;
            if (has_json_body()) {
                oss << "JSON消息体: "
                    << (json_body_.length() > 100 ? json_body_.substr(0, 100) + "..."
                                                  : json_body_)
                    << std::endl;
            }
        } else {
            oss << "Protobuf消息: " << (has_protobuf_message() ? "有" : "无") << std::endl;
            if (has_raw_protobuf_data()) {
                oss << "原始数据大小: " << raw_protobuf_data_.size() << " bytes" << std::endl;
            }
        }

        // 时间信息
        auto time_t = std::chrono::system_clock::to_time_t(session_context_.receive_time);
        oss << "接收时间: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
            << std::endl;
        oss << "===================" << std::endl;
        
        return oss;
    };

private:
    // 核心数据（基于你现有的设计）
    im::base::IMHeader header_;  // 最重要：包含cmd_id, token等
    std::unique_ptr<google::protobuf::Message> protobuf_message_;  // Protobuf消息体
    std::string json_body_;                                        // JSON消息体（HTTP用）
    std::string raw_protobuf_data_;                                // 原始Protobuf数据（避免不必要的对象创建）
    SessionContext session_context_;                               // 会话上下文
};



}  // namespace gateway
}  // namespace im

#endif  // UNIFIED_MESSAGE_HPP