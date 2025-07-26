#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/message.h>
#include <cstring>
#include "protobuf_codec.hpp"
// 包含Protobuf定义
#include "../proto/base.pb.h"  // 正确的BaseResponse定义路径
#include "../utils/global.hpp"
#include "../utils/log_manager.hpp"

using namespace google::protobuf;
using namespace google::protobuf::io;

// CRC32查找表 (IEEE标准多项式 0xEDB88320)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

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
 */
bool ProtobufCodec::encode(const im::base::IMHeader& header,
                           const google::protobuf::Message& message,
                           std::string& output) {
    try {
        // 1. 序列化消息头
        std::string header_data;
        if (!header.SerializeToString(&header_data)) {
            LogManager::GetLogger("protobuf_codec")->error("Failed to serialize IMHeader");
            return false;
        }

        // 2. 序列化消息体
        std::string message_data;
        if (!message.SerializeToString(&message_data)) {
            LogManager::GetLogger("protobuf_codec")->error("Failed to serialize message");
            return false;
        }

        // 3. 获取消息类型名称
        std::string message_type = message.GetTypeName();

        // 4. 计算消息头长度 (varint32格式)
        const uint32_t header_size = static_cast<uint32_t>(header_data.size());
        const uint32_t type_name_size = static_cast<uint32_t>(message_type.size());
        
        // 5. 预分配输出缓冲区
        output.clear();
        // 为varint预留额外空间 + 4字节CRC32
        output.reserve(header_data.size() + message_data.size() + message_type.size() + 20 + 4);

        // 6. 编码header_size
        char header_size_buffer[10];
        ArrayOutputStream array_stream(header_size_buffer, sizeof(header_size_buffer));
        CodedOutputStream coded_stream(&array_stream);
        coded_stream.WriteVarint32(header_size);
        coded_stream.Trim();
        output.append(header_size_buffer, coded_stream.ByteCount());

        // 7. 编码消息类型名称长度和内容
        char type_name_size_buffer[10];
        ArrayOutputStream type_name_stream(type_name_size_buffer, sizeof(type_name_size_buffer));
        CodedOutputStream type_name_coded_stream(&type_name_stream);
        type_name_coded_stream.WriteVarint32(type_name_size);
        type_name_coded_stream.Trim();
        output.append(type_name_size_buffer, type_name_coded_stream.ByteCount());
        output.append(message_type);

        // 8. 追加header和message数据
        output.append(header_data);
        output.append(message_data);
        
        // 9. 计算并追加CRC32校验码
        uint32_t crc = calculateCRC32(output.data(), output.size());
        char crc_buffer[4];
        memcpy(crc_buffer, &crc, 4);
        output.append(crc_buffer, 4);

        LogManager::GetLogger("protobuf_codec")->debug("Encoded data size: {}", output.size());
        LogManager::GetLogger("protobuf_codec")->debug("Header size: {}, Type name: {}, Message data size: {}", 
                                                      header_size, message_type, message_data.size());

        return true;
    } catch (const std::exception& e) {
        LogManager::GetLogger("protobuf_codec")->error("Encode error: {}", e.what());
        return false;
    }
}

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
 */
bool ProtobufCodec::decode(const std::string& input,
                           im::base::IMHeader& header,
                           google::protobuf::Message& message) {
    try {
        // 0. 检查输入数据是否为空或太小
        if (input.empty() || input.size() < 4) { // 至少需要4字节CRC
            LogManager::GetLogger("protobuf_codec")->warn("Empty or too small input data");
            return false;
        }

        LogManager::GetLogger("protobuf_codec")->debug("Input data size: {}", input.size());
        
        // 1. 验证CRC32校验码
        uint32_t received_crc;
        memcpy(&received_crc, input.data() + input.size() - 4, 4);
        uint32_t calculated_crc = calculateCRC32(input.data(), input.size() - 4);
        
        if (received_crc != calculated_crc) {
            LogManager::GetLogger("protobuf_codec")->error("CRC32 verification failed. Expected: {}, Received: {}", 
                                                          calculated_crc, received_crc);
            return false;
        }

        // 2. 使用CodedInputStream高效读取（不包括CRC部分）
        std::string data_without_crc = input.substr(0, input.size() - 4);
        ArrayInputStream array_in(data_without_crc.data(), static_cast<int>(data_without_crc.size()));
        CodedInputStream coded_in(&array_in);
        
        // 设置一个合理的限制，防止恶意构造的数据
        coded_in.SetTotalBytesLimit(64 << 20);  // 64MB

        // 3. 读取消息头长度 (varint32格式)
        uint32_t header_size = 0;
        if (!coded_in.ReadVarint32(&header_size)) {
            LogManager::GetLogger("protobuf_codec")->error("Failed to read header size");
            return false;
        }

        // 4. 读取消息类型名称长度
        uint32_t type_name_size = 0;
        if (!coded_in.ReadVarint32(&type_name_size)) {
            LogManager::GetLogger("protobuf_codec")->error("Failed to read type name size");
            return false;
        }

        // 5. 检查长度有效性
        if (header_size == 0) {
            LogManager::GetLogger("protobuf_codec")->error("Invalid header size: {}", header_size);
            return false;
        }
        
        if (type_name_size == 0) {
            LogManager::GetLogger("protobuf_codec")->error("Invalid type name size: {}", type_name_size);
            return false;
        }
        
        // 计算剩余可用字节数
        int after_header_and_type_sizes_pos = coded_in.CurrentPosition();
        int remaining_bytes = static_cast<int>(data_without_crc.size()) - after_header_and_type_sizes_pos;
        if ((header_size + type_name_size) > static_cast<uint32_t>(remaining_bytes)) {
            LogManager::GetLogger("protobuf_codec")->error("Header and type name size {} exceeds available data {}", 
                                                          (header_size + type_name_size), remaining_bytes);
            return false;
        }

        // 6. 读取消息类型名称
        std::string type_name_data(data_without_crc.data() + after_header_and_type_sizes_pos, type_name_size);
        if (!coded_in.Skip(type_name_size)) {
            LogManager::GetLogger("protobuf_codec")->error("Failed to skip type name data");
            return false;
        }

        // 7. 读取消息头数据
        std::string header_data(data_without_crc.data() + after_header_and_type_sizes_pos + type_name_size, header_size);
        
        // 移动coded stream的指针
        if (!coded_in.Skip(header_size)) {
            LogManager::GetLogger("protobuf_codec")->error("Failed to skip header data");
            return false;
        }

        // 8. 解析消息头
        if (!header.ParseFromString(header_data)) {
            LogManager::GetLogger("protobuf_codec")->error("Failed to parse IMHeader");
            return false;
        }

        // 9. 计算剩余数据大小并读取消息体
        int message_position = coded_in.CurrentPosition();
        int message_size = static_cast<int>(data_without_crc.size()) - message_position;
        
        LogManager::GetLogger("protobuf_codec")->debug("Message position: {}, Message size: {}", 
                                                      message_position, message_size);
        
        // 10. 解析消息体
        if (message_size > 0) {
            // 在解析前先检查消息体是否可能有效
            if (!message.ParseFromArray(data_without_crc.data() + message_position, message_size)) {
                LogManager::GetLogger("protobuf_codec")->error("Failed to parse message body");
                return false;
            }
            
            // 验证解析后的消息是否完整（检查所有必需字段是否都已设置）
            if (!message.IsInitialized()) {
                LogManager::GetLogger("protobuf_codec")->error("Message is not fully initialized");
                return false;
            }
            
            // 验证消息类型是否匹配
            std::string expected_type_name = message.GetTypeName();
            if (type_name_data != expected_type_name) {
                LogManager::GetLogger("protobuf_codec")->error("Message type mismatch. Expected: {}, Received: {}", 
                                                              expected_type_name, type_name_data);
                return false;
            }
        }
        
        // 11. 验证header是否完整
        if (!header.IsInitialized()) {
            LogManager::GetLogger("protobuf_codec")->error("Header is not fully initialized");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LogManager::GetLogger("protobuf_codec")->error("Decode error: {}", e.what());
        return false;
    }
}

/**
 * @brief 计算数据的CRC32校验值
 * 
 * 使用标准的CRC32算法计算给定数据的校验值。
 * 
 * @param data 数据指针
 * @param size 数据大小
 * @return CRC32校验值
 */
uint32_t ProtobufCodec::calculateCRC32(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xffffffff;
    
    for (size_t i = 0; i < size; ++i) {
        uint8_t index = (crc ^ bytes[i]) & 0xff;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    
    return crc ^ 0xffffffff;
}