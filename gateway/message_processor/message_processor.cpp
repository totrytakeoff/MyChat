#include "common/utils/log_manager.hpp"
#include "message_processor.hpp"


namespace im::gateway {

using im::gateway::MessageParser;
using im::gateway::MultiPlatformAuthManager;
using im::gateway::RouterManager;
using im::gateway::UnifiedMessage;
using im::utils::LogManager;
MessageProcessor::MessageProcessor(std::shared_ptr<RouterManager> router_mgr,
                                   std::shared_ptr<MultiPlatformAuthManager>
                                           auth_mgr)
        : router_mgr_(router_mgr), auth_mgr_(auth_mgr) {
    LogManager::GetLogger("message_processor")
            ->info("MessageProcessor initialized with existing RouterManager and AuthManager");
}


MessageProcessor::MessageProcessor(std::string& router_config_file, std::string& auth_config_file) {
    router_mgr_ = std::make_unique<RouterManager>(router_config_file);
    auth_mgr_ = std::make_unique<MultiPlatformAuthManager>(auth_config_file);
    LogManager::GetLogger("message_processor")
            ->info("MessageProcessor initialized with new RouterManager and AuthManager");
}

int MessageProcessor::register_processor(
        uint32_t cmd_id, std::function<ProcessorResult(const UnifiedMessage&)> processor) {
    auto service = router_mgr_->find_service(cmd_id);
    if (!service || !service->is_valid) {
        LogManager::GetLogger("message_processor")
                ->error("MessageProcessor::register_processor: service not found for cmd_id: {}",
                        cmd_id);
        return -1;
    }

    if (processor_map_.contains(cmd_id)) {
        LogManager::GetLogger("message_processor")
                ->warn("MessageProcessor::register_processor: processor already exists for cmd_id: "
                       "{}",
                       cmd_id);
        return 1;
    }
    processor_map_[cmd_id] = processor;
    LogManager::GetLogger("message_processor")
            ->info("MessageProcessor::register_processor: processor registered for cmd_id: {}",
                   cmd_id);
    return 0;
}

std::future<ProcessorResult> MessageProcessor::process_message(
        std::unique_ptr<UnifiedMessage> message) {
    return std::future<ProcessorResult>();
}


}  // namespace im::gateway
