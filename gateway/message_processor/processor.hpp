#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP


#include <chrono>
#include <memory>
#include <string>

#include <httplib.h>
#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/utils/log_manager.hpp"
#include "http_router.hpp"
#include "unified_message.hpp"


namespace im {

namespace gateway {

using im::gateway::HttpRouter;
using im::gateway::UnifiedMessage;
/**
 * @class SimpleHttpProcessor
 * @brief 简化的HTTP处理器 - 基于你的HttpRouter
 */
class SimpleHttpProcessor {
public:
    SimpleHttpProcessor();

    bool load_config(const std::string& config_file);

    // 核心处理方法：HTTP请求 -> UnifiedMessage
    std::unique_ptr<UnifiedMessage> process_http_request(const httplib::Request& req,
                                                         const std::string& session_id = "");

    // WebSocket消息处理（利用现有的protobuf解码）
    std::unique_ptr<UnifiedMessage> process_websocket_message(const std::string& raw_message,
                                                              const std::string& session_id = "");

private:
    // 使用你现有的HttpRouter
    std::unique_ptr<HttpRouter> http_router_;

    // 复用你现有的protobuf编解码器
    im::network::ProtobufCodec protobuf_codec_;

    // 辅助方法
    std::string extract_token(const httplib::Request& req);
    std::string extract_device_id(const httplib::Request& req);
    std::string extract_platform(const httplib::Request& req);
    std::string extract_client_ip(const httplib::Request& req);
    std::string generate_session_id();

    static int session_counter_;
};
}  // namespace gateway
}  // namespace im



#endif  // PROCESSOR_HPP