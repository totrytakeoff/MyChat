#include "message_ws_handler.hpp"

#include <chrono>
#include <string>

#include "../../common/network/packet_error_builder.hpp"
#include "../../common/network/protobuf_codec.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../../common/utils/service_identity.hpp"
#include "../../services/message/message_packet_dispatcher.hpp"
#include "../../services/message/message_service.hpp"
#include "../../services/push/push_notifier.hpp"
#include "../service_adapters/message_service_adapter.hpp"

namespace im::gateway {

using im::base::ErrorCode;
using im::network::PacketErrorBuilder;
using im::network::ProtobufCodec;
using im::utils::LogManager;
using im::utils::ServiceIdentityManager;

namespace {

std::string encode_error_response(const im::base::IMHeader& request_header,
                                  ErrorCode error_code,
                                  const std::string& error_message)
{
    return PacketErrorBuilder::build_json_error_response(
        request_header,
        error_code,
        error_message,
        ServiceIdentityManager::getInstance().getDeviceId(),
        ServiceIdentityManager::getInstance().getPlatformInfo());
}

} // anonymous namespace

MessageWsHandler::MessageWsHandler(
    std::shared_ptr<im::service::message::MessageService> message_service,
    std::shared_ptr<MessageServiceAdapter> remote_message_adapter,
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
    im::service::push::PushNotifier* push_notifier)
    : message_service_(std::move(message_service))
    , remote_message_adapter_(std::move(remote_message_adapter))
    , auth_mgr_(std::move(auth_mgr))
    , push_notifier_(push_notifier)
{
    LogManager::SetLogToFile("message_ws_handler", "logs/message_ws_handler.log");
    logger_ = LogManager::GetLogger("message_ws_handler");
    if (message_service_) {
        local_dispatcher_ =
                std::make_unique<im::service::message::MessagePacketDispatcher>(
                        message_service_.get());
    }
}

ProcessorResult MessageWsHandler::handle_send(const UnifiedMessage& msg) {
    try {
        if (!msg.is_websocket()) {
            return ProcessorResult(ErrorCode::INVALID_REQUEST,
                                   "WS handler called for non-WebSocket message",
                                   "", "");
        }

        const auto& header = msg.get_header();

        // 1. Validate cmd_id.
        if (msg.get_cmd_id() != im::command::CMD_SEND_MESSAGE) {
            logger_->warn("Unexpected cmd_id: {}, expected CMD_SEND_MESSAGE(2001)",
                          msg.get_cmd_id());
            std::string error_pb = encode_error_response(
                header, ErrorCode::INVALID_REQUEST,
                "Unexpected command ID, expected CMD_SEND_MESSAGE");
            return ProcessorResult(ErrorCode::INVALID_REQUEST,
                                   "Unexpected cmd_id", error_pb, "");
        }

        // 2. Verify the access token and derive sender identity.
        UserTokenInfo token_user;
        if (!auth_mgr_->verify_access_token(header.token(), token_user)) {
            std::string error_pb = encode_error_response(
                header, ErrorCode::AUTH_FAILED,
                "Invalid or expired access token");
            return ProcessorResult(ErrorCode::AUTH_FAILED,
                                   "Invalid or expired access token", error_pb, "");
        }

        // 3. Reject if the client-supplied device_id does not match the token.
        if (!header.device_id().empty() && header.device_id() != token_user.device_id) {
            logger_->warn("Device ID mismatch: client={}, token={}",
                          header.device_id(), token_user.device_id);
            std::string error_pb = encode_error_response(
                header, ErrorCode::AUTH_FAILED,
                "Device ID mismatch");
            return ProcessorResult(ErrorCode::AUTH_FAILED,
                                   "Device ID mismatch", error_pb, "");
        }

        // 4. Validate routing-level receiver. Payload-level validation belongs to message service.
        std::string receiver_uid = header.to_uid();
        if (receiver_uid.empty()) {
            std::string error_pb = encode_error_response(
                header, ErrorCode::PARAM_ERROR,
                "Missing receiver UID");
            return ProcessorResult(ErrorCode::PARAM_ERROR,
                                   "Missing receiver UID", error_pb, "");
        }

        if (!remote_message_adapter_ && !local_dispatcher_) {
            std::string error_pb = encode_error_response(
                header, ErrorCode::SERVER_ERROR, "Message service is not initialized");
            return ProcessorResult(ErrorCode::SERVER_ERROR,
                                   "Message service is not initialized", error_pb, "");
        }

        im::base::IMHeader service_header = header;
        service_header.set_from_uid(token_user.user_id);
        service_header.set_to_uid(receiver_uid);

        im::message::MessagePacketRequest packet;
        packet.mutable_header()->CopyFrom(service_header);
        packet.set_type_name(msg.get_protobuf_type_name());
        packet.set_payload(msg.get_protobuf_payload());

        im::message::MessagePacketResponse result;
        if (remote_message_adapter_) {
            result = remote_message_adapter_->forward(packet);
        } else {
            result = local_dispatcher_->handle(packet);
        }

        std::string protobuf_response;
        if (!ProtobufCodec::encodeEnvelope(result.header(),
                                           result.type_name(),
                                           result.payload(),
                                           protobuf_response)) {
            logger_->error("Failed to encode message packet response");
            std::string error_pb = encode_error_response(
                header, ErrorCode::SERVER_ERROR, "Failed to encode response");
            return ProcessorResult(ErrorCode::SERVER_ERROR,
                                   "Failed to encode response", error_pb, "");
        }

        const bool ok = result.base().error_code() == im::base::SUCCESS;
        if (!ok) {
            logger_->warn("Message service packet failed: {} ({})",
                          result.base().error_message(),
                          static_cast<int>(result.base().error_code()));
            return ProcessorResult(result.base().error_code(),
                                   result.base().error_message(),
                                   protobuf_response,
                                   "");
        }

        logger_->info("WS send packet forwarded: token_user={} -> {} (client_from_uid={})",
                      token_user.user_id, receiver_uid, header.from_uid());

        if (push_notifier_ && result.push_event().enabled()) {
            try {
                im::service::push::PushContext push_context;
                push_context.sender_uid = result.push_event().sender_uid();
                push_context.conversation_type = result.push_event().conversation_type();
                push_context.conversation_id = result.push_event().conversation_id();
                push_notifier_->notify_user(result.push_event().receiver_uid(),
                                            result.push_event().msg_id(),
                                            result.push_event().content(),
                                            push_context);
            } catch (const std::exception& e) {
                logger_->warn("Push notification failed after WS send ack: {}", e.what());
            }
        }

        return ProcessorResult(0, "", protobuf_response, "");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_send: {}", e.what());
        return ProcessorResult(ErrorCode::SERVER_ERROR,
                               std::string("Exception: ") + e.what(), "", "");
    }
}

} // namespace im::gateway
