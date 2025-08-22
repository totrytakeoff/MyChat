#ifndef SIMPLE_MESSAGE_HPP
#define SIMPLE_MESSAGE_HPP

/******************************************************************************
 *
 * @file       simple_message.hpp
 * @brief      简单的消息格式定义
 *
 * @author     myself
 * @date       2025/01/20
 *
 *****************************************************************************/

#include <string>
#include <unordered_map>
#include <chrono>

namespace im {
namespace gateway {

/**
 * @struct SimpleMessage
 * @brief 简单的通用消息格式
 */
struct SimpleMessage {
    // 基本信息
    uint32_t cmd_id;              // 命令ID
    std::string service_name;     // 目标服务名称
    std::string session_id;       // 会话ID
    
    // HTTP请求信息
    std::string method;           // HTTP方法：GET, POST等
    std::string path;             // 请求路径
    std::string body;             // 请求体（JSON字符串）
    
    // 认证信息
    std::string token;            // 用户token
    std::string device_id;        // 设备ID
    std::string platform;         // 平台信息
    
    // 其他信息
    std::string client_ip;        // 客户端IP
    std::chrono::system_clock::time_point timestamp;  // 时间戳
    
    // 构造函数
    SimpleMessage() : cmd_id(0), timestamp(std::chrono::system_clock::now()) {}
    
    // 打印消息信息（调试用）
    void print() const;
};

/**
 * @struct SimpleResponse
 * @brief 简单的响应格式
 */
struct SimpleResponse {
    int status_code;              // HTTP状态码
    std::string content_type;     // 内容类型
    std::string body;             // 响应体
    bool success;                 // 是否成功
    std::string error_message;    // 错误信息
    
    // 构造函数
    SimpleResponse() : status_code(200), content_type("application/json"), success(true) {}
    
    // 创建成功响应
    static SimpleResponse create_success(const std::string& data);
    
    // 创建错误响应
    static SimpleResponse create_error(int code, const std::string& message);
};

}  // namespace gateway
}  // namespace im

#endif  // SIMPLE_MESSAGE_HPP