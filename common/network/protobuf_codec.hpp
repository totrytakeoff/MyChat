#ifndef PROTOBUF_CODEC_HPP
#define PROTOBUF_CODEC_HPP

#include <string>
#include <google/protobuf/message.h>
#include "../proto/base.pb.h"

namespace im {
namespace network {

/**
 * @brief Protobuf编解码器类
 * 
 * 该类负责将IMHeader和Protobuf消息序列化为二进制数据，以及将二进制数据反序列化为IMHeader和Protobuf消息。
 * 
 * 数据格式：
 * [header_size(varint)][type_name_size(varint)][type_name_string][header_data][message_data][CRC32]
 * 
 * 特性：
 * 1. 使用varint编码存储头部大小和类型名称大小
 * 2. 在消息中包含类型信息以支持类型验证
 * 3. 使用CRC32校验确保数据完整性
 * 4. 支持空消息和大型消息的处理
 * 5. 提供详细的错误日志记录
 */
class ProtobufCodec {
public:
    /**
     * @brief 编码 IMHeader 和 Protobuf 消息
     * 
     * 将IMHeader和Protobuf消息序列化为二进制数据，格式为：
     * [header_size(varint)][type_name_size(varint)][type_name_string][header_data][message_data][CRC32]
     * 
     * @param header 消息头，包含版本、序列号、命令ID等元数据
     * @param message 消息体，具体的Protobuf消息
     * @param output 输出序列化后的二进制数据
     * @return 是否编码成功
     * 
     * @note 编码过程包含以下步骤：
     * 1. 序列化消息头和消息体
     * 2. 获取消息类型名称
     * 3. 编码各部分大小（使用varint）
     * 4. 拼接所有数据
     * 5. 计算并追加CRC32校验码
     * 
     * @see decode
     */
    static bool encode(const im::base::IMHeader& header, 
                      const google::protobuf::Message& message, 
                      std::string& output);
    
    /**
     * @brief 解码二进制数据为 IMHeader 和 Protobuf 消息
     * 
     * 将二进制数据反序列化为IMHeader和Protobuf消息。
     * 
     * @param input 输入的二进制数据，格式应为：
     *              [header_size(varint)][type_name_size(varint)][type_name_string][header_data][message_data][CRC32]
     * @param header 输出解析后的消息头
     * @param message 输出解析后的消息体
     * @return 是否解码成功
     * 
     * @note 解码过程包含以下步骤：
     * 1. 验证CRC32校验码
     * 2. 读取头部大小和类型名称大小
     * 3. 读取类型名称、头部数据和消息体数据
     * 4. 解析头部和消息体
     * 5. 验证消息类型是否匹配
     * 6. 检查消息和头部是否完全初始化
     * 
     * @see encode
     */
    static bool decode(const std::string& input, 
                      im::base::IMHeader& header, 
                      google::protobuf::Message& message);

    // 根据请求header构建返回header
    static base::IMHeader returnHeaderBuilder(base::IMHeader header,std::string device_id,std::string platform);
    
    /**
     * @brief 构建认证失败的protobuf响应消息
     * @param request_header 请求头（用于构建响应头）
     * @param error_message 错误消息
     * @return 编码后的protobuf二进制数据
     */
    static std::string buildAuthFailedResponse(const base::IMHeader& request_header, 
                                              const std::string& error_message = "Authentication failed");
    
    /**
     * @brief 构建超时的protobuf响应消息
     * @param request_header 请求头（用于构建响应头）
     * @param error_message 错误消息
     * @return 编码后的protobuf二进制数据
     */
    static std::string buildTimeoutResponse(const base::IMHeader& request_header,
                                           const std::string& error_message = "Authentication timeout");
    
    /**
     * @brief 构建通用错误的protobuf响应消息
     * @param request_header 请求头（用于构建响应头）
     * @param error_code 错误码
     * @param error_message 错误消息
     * @return 编码后的protobuf二进制数据
     */
    static std::string buildErrorResponse(const base::IMHeader& request_header,
                                         base::ErrorCode error_code,
                                         const std::string& error_message);
private:
    /**
     * @brief 计算数据的CRC32校验值
     * 
     * 使用标准的CRC32算法计算给定数据的校验值。
     * 
     * @param data 数据指针
     * @param size 数据大小
     * @return CRC32校验值
     * 
     * @note 使用IEEE标准的CRC32多项式0xEDB88320
     */
    static uint32_t calculateCRC32(const void* data, size_t size);
};

} // namespace network
} // namespace im

#endif // PROTOBUF_CODEC_HPP