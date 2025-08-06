#pragma once

#include <string>
#include <memory>
#include <chrono>
#include "common/proto/base.pb.h"
#include "common/proto/command.proto"
#include "common/utils/json.hpp"
#include "common/utils/log_manager.hpp"

using json = nlohmann::json;

/**
 * @brief 消息处理器 - 负责解析和封装客户端与服务端之间的消息
 * 
 * 主要功能：
 * 1. 解析WebSocket消息（Protobuf格式）
 * 2. 解析HTTP请求（JSON格式）
 * 3. 统一封装响应消息
 * 4. 消息格式验证
 */
class MessageProcessor {
public:
    MessageProcessor();
    ~MessageProcessor();
    
    /**
     * @brief 解析后的消息结构
     */
    struct ParsedMessage {
        im::base::IMHeader header;          // 消息头
        std::string payload;                // 消息载荷（JSON字符串）
        bool valid = false;                 // 是否有效
        std::string error_msg;              // 错误信息
        
        ParsedMessage() = default;
        ParsedMessage(bool is_valid) : valid(is_valid) {}
    };
    
    // ==================== 消息解析接口 ====================
    
    /**
     * @brief 解析WebSocket消息（期望是JSON格式，包含header和payload）
     * @param raw_data 原始WebSocket数据
     * @return 解析结果
     */
    ParsedMessage ParseWebSocketMessage(const std::string& raw_data);
    
    /**
     * @brief 解析HTTP请求，转换为内部消息格式
     * @param req_body HTTP请求体（JSON格式）
     * @param cmd_id 命令ID
     * @param user_token 用户Token（从HTTP Header获取）
     * @return 解析结果
     */
    ParsedMessage ParseHttpRequest(const std::string& req_body, 
                                  uint32_t cmd_id, 
                                  const std::string& user_token = "");
    
    // ==================== 消息封装接口 ====================
    
    /**
     * @brief 封装WebSocket响应消息
     * @param seq 序列号（对应请求的seq）
     * @param cmd_id 命令ID
     * @param response 响应数据
     * @return JSON格式的响应字符串
     */
    std::string PackWebSocketResponse(uint32_t seq, 
                                    uint32_t cmd_id,
                                    const im::base::BaseResponse& response);
    
    /**
     * @brief 封装WebSocket推送消息
     * @param to_user 目标用户ID
     * @param cmd_id 推送命令ID
     * @param payload 推送内容（JSON字符串）
     * @return JSON格式的推送消息
     */
    std::string PackPushMessage(const std::string& to_user,
                              uint32_t cmd_id,
                              const std::string& payload);
    
    /**
     * @brief 将BaseResponse转换为HTTP响应的JSON格式
     * @param response 服务响应
     * @return HTTP响应JSON字符串
     */
    std::string BaseResponseToHttpJson(const im::base::BaseResponse& response);
    
    // ==================== 工具方法 ====================
    
    /**
     * @brief 创建错误响应
     * @param error_code 错误码
     * @param error_message 错误信息
     * @param payload 附加数据（可选）
     * @return BaseResponse对象
     */
    static im::base::BaseResponse CreateErrorResponse(
        im::base::ErrorCode error_code,
        const std::string& error_message,
        const std::string& payload = "");
    
    /**
     * @brief 创建成功响应
     * @param payload 响应数据
     * @return BaseResponse对象
     */
    static im::base::BaseResponse CreateSuccessResponse(const std::string& payload);
    
    /**
     * @brief 获取当前时间戳（毫秒）
     * @return 时间戳
     */
    static uint64_t GetCurrentTimestamp();

private:
    /**
     * @brief 验证消息头的有效性
     * @param header 消息头
     * @return 是否有效
     */
    bool ValidateHeader(const im::base::IMHeader& header);
    
    /**
     * @brief 验证命令ID是否合法
     * @param cmd_id 命令ID
     * @return 是否合法
     */
    bool ValidateCommand(uint32_t cmd_id);
    
    /**
     * @brief 从JSON中解析IMHeader
     * @param header_json JSON对象
     * @param header 输出的header对象
     * @return 是否成功
     */
    bool ParseHeaderFromJson(const json& header_json, im::base::IMHeader& header);
    
    // 日志管理器
    std::shared_ptr<LogManager> log_mgr_;
};