#ifndef PROTOBUF_CODEC_HPP
#define PROTOBUF_CODEC_HPP

/******************************************************************************
 *
 * @file       protobuf_codec.hpp
 * @brief      protobuf编解码
 *
 * @author     myself
 * @date       2025/07/23
 *
 *****************************************************************************/

#include <google/protobuf/message.h>
#include <string>
#include <functional>
#include "../proto/base.pb.h"  // 正确的BaseResponse定义路径
#include "tcp_session.hpp"

class ProtobufCodec {
public:
    using IMHeader = im::base::IMHeader;
    using MessageHandler =
            std::function<void(const IMHeader&, const std::string&, TCPSession::Ptr)>;

    // 发送消息
    template <typename T>
    static void send(const T& message, TCPSession::Ptr session);

    // 接收完整消息
    static void on_message(TCPSession::Ptr session, const std::string& data, size_t len);

    static void set_message_handler(MessageHandler callback);

private:
    static MessageHandler message_handler_;
    static void handle_message(const IMHeader& header, const std::string& payload, TCPSession::Ptr session);
};

// 显式实例化模板
extern template void ProtobufCodec::send(const IMHeader&, TCPSession::Ptr);
extern template void ProtobufCodec::send(const BaseResponse&, TCPSession::Ptr);

#endif  // PROTOBUF_CODEC_HPP