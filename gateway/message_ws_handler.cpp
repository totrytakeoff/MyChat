#include "message_ws_handler.hpp"

#include <chrono>
#include <string>

#include "../common/network/protobuf_codec.hpp"
#include "../common/utils/log_manager.hpp"
#include "../common/utils/service_identity.hpp"
#include "../services/message/message_service.hpp"
#include "../services/odb/message.hpp"

namespace im::gateway {

using im::base::ErrorCode;
using im::network::ProtobufCodec;
using im::service::message::MessageService;
using im::service::message::SendRequest;
using im::utils::LogManager;
using im::utils::ServiceIdentityManager;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Build an encoded SendMessageResponse with the given error code and message,
// preserving the request header's sequence identity.
std::string encode_error_response(const im::base::IMHeader& request_header,
                                  ErrorCode error_code,
                                  const std::string& error_message)
{
    im::message::SendMessageResponse resp;
    resp.mutable_base()->set_error_code(error_code);
    if (!error_message.empty()) {
        resp.mutable_base()->set_error_message(error_message);
    }

    base::IMHeader response_header = ProtobufCodec::returnHeaderBuilder(
        request_header,
        ServiceIdentityManager::getInstance().getDeviceId(),
        ServiceIdentityManager::getInstance().getPlatformInfo());

    std::string encoded;
    if (ProtobufCodec::encode(response_header, resp, encoded)) {
        return encoded;
    }
    return "";
}

} // anonymous namespace

MessageWsHandler::MessageWsHandler(
    std::shared_ptr<MessageService> msg_service,
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr)
    : msg_service_(std::move(msg_service))
    , auth_mgr_(std::move(auth_mgr))
{
    LogManager::SetLogToFile("message_ws_handler", "logs/message_ws_handler.log");
    logger_ = LogManager::GetLogger("message_ws_handler");
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
        //    We trust the token, not the client-supplied from_uid.
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

        // 4. Validate protobuf type name.
        if (msg.get_protobuf_type_name() != "im.message.SendMessageRequest") {
            logger_->warn("Unexpected protobuf type: {}, expected im.message.SendMessageRequest",
                          msg.get_protobuf_type_name());
            std::string error_pb = encode_error_response(
                header, ErrorCode::INVALID_REQUEST,
                "Unexpected protobuf type, expected im.message.SendMessageRequest");
            return ProcessorResult(ErrorCode::INVALID_REQUEST,
                                   "Wrong protobuf type", error_pb, "");
        }

        // 5. Parse the SendMessageRequest from the protobuf payload.
        im::message::SendMessageRequest send_req;
        if (!send_req.ParseFromString(msg.get_protobuf_payload())) {
            std::string error_pb = encode_error_response(
                header, ErrorCode::INVALID_REQUEST,
                "Failed to parse SendMessageRequest");
            return ProcessorResult(ErrorCode::INVALID_REQUEST,
                                   "Failed to parse SendMessageRequest", error_pb, "");
        }

        // 6. Validate receiver.
        std::string receiver_uid = header.to_uid();
        if (receiver_uid.empty()) {
            std::string error_pb = encode_error_response(
                header, ErrorCode::PARAM_ERROR,
                "Missing receiver UID");
            return ProcessorResult(ErrorCode::PARAM_ERROR,
                                   "Missing receiver UID", error_pb, "");
        }

        // 7. Validate content (from the protobuf body, or fall back to header).
        const auto& body = send_req.body();
        std::string content = body.content();
        if (content.empty()) {
            std::string error_pb = encode_error_response(
                header, ErrorCode::PARAM_ERROR,
                "Missing message content");
            return ProcessorResult(ErrorCode::PARAM_ERROR,
                                   "Missing message content", error_pb, "");
        }

        // 8. Persist the message. The sender identity comes from the verified
        //    token, NOT from header.from_uid or any client-supplied field.
        SendRequest store_req;
        store_req.sender_uid = token_user.user_id;
        store_req.receiver_uid = receiver_uid;
        store_req.content = content;
        store_req.msg_type = im::service::message::MessageType::TEXT;
        store_req.now_ms = now_ms();

        auto result = msg_service_->send_text_message(store_req);
        if (!result.ok) {
            logger_->warn("Persistence failed: {} ({})", result.message, result.error_code);
            std::string error_pb = encode_error_response(
                header, ErrorCode::SERVER_ERROR, result.message);
            return ProcessorResult(ErrorCode::SERVER_ERROR, result.message, error_pb, "");
        }

        // 9. Build SendMessageResponse protobuf.
        im::message::SendMessageResponse send_resp;
        send_resp.mutable_base()->set_error_code(ErrorCode::SUCCESS);
        send_resp.mutable_base()->set_error_message("");
        im::message::MessageBody* response_body = send_resp.mutable_message();
        response_body->set_message_id(std::to_string(result.data.msg_id));
        response_body->set_type(static_cast<im::message::MessageType>(result.data.msg_type));
        response_body->set_content(result.data.content);
        response_body->set_is_recalled(false);
        response_body->set_is_read(false);

        base::IMHeader response_header = ProtobufCodec::returnHeaderBuilder(
            header,
            ServiceIdentityManager::getInstance().getDeviceId(),
            ServiceIdentityManager::getInstance().getPlatformInfo());

        std::string protobuf_response;
        if (!ProtobufCodec::encode(response_header, send_resp, protobuf_response)) {
            logger_->error("Failed to encode SendMessageResponse");
            std::string error_pb = encode_error_response(
                header, ErrorCode::SERVER_ERROR, "Failed to encode response");
            return ProcessorResult(ErrorCode::SERVER_ERROR,
                                   "Failed to encode response", error_pb, "");
        }

        logger_->info("WS send: token_user={} -> {} (msg_id={}, client_from_uid={})",
                      token_user.user_id, receiver_uid, result.data.msg_id, header.from_uid());
        return ProcessorResult(0, "", protobuf_response, "");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_send: {}", e.what());
        return ProcessorResult(ErrorCode::SERVER_ERROR,
                               std::string("Exception: ") + e.what(), "", "");
    }
}

} // namespace im::gateway
