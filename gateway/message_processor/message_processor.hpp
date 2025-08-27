#ifndef MESSAGE_PROCESSOR_HPP
#define MESSAGE_PROCESSOR_HPP

/******************************************************************************
 *
 * @file       message_processor.hpp
 * @brief      异步消息处理器，用于处理解析后的消息
 *
 * @author     myself
 * @date       2025/08/27
 *
 *****************************************************************************/

#include "../auth/multi_platform_auth.hpp"
#include "../router/router_mgr.hpp"
#include "common/proto/base.pb.h"
#include "message_parser.hpp"
#include "unified_message.hpp"

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>

namespace im::gateway {
using im::gateway::MessageParser;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::RouterManager;
using im::gateway::UnifiedMessage;


struct ProcessorResult {
    int status_code;
    std::string error_message;
    std::string protobuf_message;
    std::string json_body;
};

class MessageProcessor {
public:
    MessageProcessor(std::shared_ptr<RouterManager> router_mgr,
                     std::shared_ptr<MultiPlatformAuthManager>
                             auth_mgr);

    MessageProcessor(std::string& router_config_file, std::string& auth_config_file);
    MessageProcessor(const MessageProcessor&) = delete;
    MessageProcessor& operator=(const MessageProcessor&) = delete;


    // 0 -> success
    // 1 -> callback exists
    // -1 -> cmd not found
    // other -> error code

    int register_processor(
            uint32_t cmd_id, std::function<ProcessorResult(const UnifiedMessage&)> processor);

    std::future<ProcessorResult> process_message(std::unique_ptr<UnifiedMessage> message);

    int get_callback_count() const { return processor_map_.size(); }

private:
    std::shared_ptr<RouterManager> router_mgr_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::unordered_map<uint32_t, std::function<ProcessorResult(const UnifiedMessage&)>>
            processor_map_;
};



}  // namespace im::gateway



#endif  // MESSAGE_PROCESSOR_HPP