#include <google/protobuf/message.h>
#include <boost/endian/conversion.hpp>
#include <spdlog/spdlog.h>
#include "protobuf_codec.hpp"
#include "tcp_session.hpp"
// 包含Protobuf定义
#include "../proto/base.pb.h"  // 正确的BaseResponse定义路径

// 使用全名空间限定BaseResponse
using im::base::BaseResponse;

// 发送消息（带消息长度前缀）
template <typename T>
void ProtobufCodec::send(const T& message, TCPSession::Ptr session) {
    std::string data;
    message.SerializeToString(&data);
    
    // 添加4字节长度前缀（使用uint32_t保证平台一致性）
    uint32_t len = static_cast<uint32_t>(data.size());
    boost::endian::native_to_big_inplace(len);  // 使用boost进行字节序转换
    
    char len_buf[4];
    std::memcpy(len_buf, &len, sizeof(len));  // 使用memcpy保证类型安全
    
    // 异步发送消息
    session->async_write(len_buf, 4, [data, session](bool success) {
        if (success) {
            session->async_write(data.data(), data.size(), [](bool success) {
                if (!success) {
                    SPDLOG_WARN("Failed to send message data");
                }
            });
        } else {
            SPDLOG_WARN("Failed to send message length prefix");
        }
    });
}

// 接收完整消息（处理粘包问题）
void ProtobufCodec::on_message(TCPSession::Ptr session, const std::string& data, size_t len) {
    // 解析完整Protobuf消息（假设data是完整消息体）
    im::base::IMHeader header;
    if (!header.ParseFromArray(data.data(), len)) {
        SPDLOG_ERROR("Parse header failed");
        session->close();
        return;
    }

    // 调用业务处理函数
    if (message_handler_) {
        message_handler_(header, data, session);
    }
}

// 显式实例化常用消息类型的send函数
template void ProtobufCodec::send(const im::base::IMHeader&, TCPSession::Ptr);
template void ProtobufCodec::send(const BaseResponse&, TCPSession::Ptr);