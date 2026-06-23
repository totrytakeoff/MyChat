#include "message_packet_dispatcher.hpp"

#include <chrono>
#include <climits>
#include <exception>
#include <string>

#include <nlohmann/json.hpp>

#include "command.pb.h"
#include "message.hpp"
#include "message.pb.h"
#include "message_service.hpp"

namespace im::service::message {

namespace {

using json = nlohmann::json;

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
}

im::base::IMHeader response_header(const im::base::IMHeader& request_header) {
    im::base::IMHeader header;
    header.set_version(request_header.version());
    header.set_seq(request_header.seq());
    header.set_cmd_id(request_header.cmd_id());
    header.set_from_uid(request_header.to_uid());
    header.set_to_uid(request_header.from_uid());
    header.set_timestamp(static_cast<uint64_t>(now_ms()));
    header.set_token(request_header.token());
    header.set_device_id(request_header.device_id());
    header.set_platform(request_header.platform());
    return header;
}

MessageType to_service_type(im::message::MessageType type) {
    switch (type) {
        case im::message::TEXT:
            return MessageType::TEXT;
        case im::message::IMAGE:
            return MessageType::IMAGE;
        case im::message::FILE:
            return MessageType::FILE;
        case im::message::SYSTEM:
            return MessageType::SYSTEM;
        default:
            return MessageType::TEXT;
    }
}

im::message::MessageType to_proto_type(MessageType type) {
    switch (type) {
        case MessageType::TEXT:
            return im::message::TEXT;
        case MessageType::IMAGE:
            return im::message::IMAGE;
        case MessageType::FILE:
            return im::message::FILE;
        case MessageType::SYSTEM:
            return im::message::SYSTEM;
        default:
            return im::message::TEXT;
    }
}

void fill_message_body(const MessageData& data, im::message::MessageBody* body) {
    body->set_message_id(std::to_string(data.msg_id));
    body->set_type(to_proto_type(data.msg_type));
    body->set_content(data.content);
    body->set_is_read(data.read_time > 0 || data.status == MessageStatus::READ);
    body->set_sender_uid(data.sender_uid);
    body->set_receiver_uid(data.receiver_uid);
    body->set_create_time(data.create_time);
    body->set_delivered_time(data.delivered_time);
    body->set_read_time(data.read_time);
}

json message_data_to_json(const MessageData& data) {
    json body;
    body["msg_id"] = data.msg_id;
    body["sender_uid"] = data.sender_uid;
    body["receiver_uid"] = data.receiver_uid;
    body["content"] = data.content;
    body["msg_type"] = static_cast<int>(data.msg_type);
    body["status"] = static_cast<int>(data.status);
    body["create_time"] = data.create_time;
    body["delivered_time"] = data.delivered_time;
    body["read_time"] = data.read_time;
    return body;
}

std::string json_error_body(int status, const std::string& message) {
    json body;
    body["code"] = status;
    body["body"] = "";
    body["err_msg"] = message;
    return body.dump();
}

void fill_packet_response(im::message::MessagePacketResponse* result,
                          const im::message::MessagePacketRequest& request,
                          im::base::ErrorCode code,
                          int http_status,
                          const std::string& message,
                          const std::string& type_name,
                          const std::string& payload) {
    result->mutable_base()->set_error_code(code);
    result->mutable_base()->set_error_message(message);
    result->mutable_header()->CopyFrom(response_header(request.header()));
    result->set_type_name(type_name);
    result->set_payload(payload);
    result->set_http_status(http_status);
}

im::message::MessagePacketResponse protobuf_error_result(const im::message::MessagePacketRequest& request,
                                          im::base::ErrorCode code,
                                          const std::string& message) {
    im::message::MessagePacketResponse result;

    im::message::SendMessageResponse response;
    response.mutable_base()->set_error_code(code);
    response.mutable_base()->set_error_message(message);
    std::string payload;
    response.SerializeToString(&payload);
    fill_packet_response(&result,
                         request,
                         code,
                         0,
                         message,
                         "im.message.SendMessageResponse",
                         payload);
    return result;
}

im::message::MessagePacketResponse json_error_result(const im::message::MessagePacketRequest& request,
                                      im::base::ErrorCode code,
                                      int http_status,
                                      const std::string& message) {
    im::message::MessagePacketResponse result;
    fill_packet_response(&result,
                         request,
                         code,
                         http_status,
                         message,
                         "application/json",
                         json_error_body(http_status, message));
    return result;
}

im::message::MessagePacketResponse error_result(const im::message::MessagePacketRequest& request,
                                 im::base::ErrorCode code,
                                 const std::string& message) {
    if (request.type_name() == "application/json") {
        const int http_status =
                (code == im::base::PARAM_ERROR || code == im::base::INVALID_REQUEST) ? 400 : 500;
        return json_error_result(request, code, http_status, message);
    }
    return protobuf_error_result(request, code, message);
}

im::base::ErrorCode send_error_code(const std::string& code) {
    if (code == "EMPTY_SENDER" || code == "EMPTY_RECEIVER" || code == "EMPTY_CONTENT") {
        return im::base::PARAM_ERROR;
    }
    return im::base::SERVER_ERROR;
}

int send_http_status(im::base::ErrorCode code) {
    switch (code) {
        case im::base::PARAM_ERROR:
        case im::base::INVALID_REQUEST:
            return 400;
        default:
            return 500;
    }
}

int parse_limit(const json& body, int default_value, int max_value) {
    int limit = default_value;
    if (body.contains("limit")) {
        if (body["limit"].is_number_integer()) {
            limit = body["limit"].get<int>();
        } else if (body["limit"].is_string()) {
            const std::string value = body["limit"].get<std::string>();
            if (!value.empty()) {
                try {
                    limit = std::stoi(value);
                } catch (...) {
                    return default_value;
                }
            }
        }
    }
    if (limit <= 0 || limit > max_value) {
        return default_value;
    }
    return limit;
}

int64_t parse_before_time(const json& body) {
    if (!body.contains("before_time")) {
        return INT64_MAX;
    }
    if (body["before_time"].is_number_integer()) {
        return body["before_time"].get<int64_t>();
    }
    if (body["before_time"].is_string()) {
        const std::string value = body["before_time"].get<std::string>();
        if (value.empty()) {
            return INT64_MAX;
        }
        return std::stoll(value);
    }
    return INT64_MAX;
}

im::message::MessagePacketResponse json_messages_result(const im::message::MessagePacketRequest& packet,
                                         const std::vector<MessageData>& messages) {
    json response;
    response["messages"] = json::array();
    for (const auto& item : messages) {
        response["messages"].push_back(message_data_to_json(item));
    }
    response["count"] = messages.size();

    im::message::MessagePacketResponse result;
    fill_packet_response(&result,
                         packet,
                         im::base::SUCCESS,
                         200,
                         "",
                         "application/json",
                         response.dump());
    return result;
}

void fill_push_event(const MessageData& data, im::message::MessagePushEvent* event) {
    if (!event) {
        return;
    }
    event->set_enabled(true);
    event->set_receiver_uid(data.receiver_uid);
    event->set_msg_id(data.msg_id);
    event->set_content(data.content);
    event->set_sender_uid(data.sender_uid);
    event->set_conversation_type("direct");
    event->set_conversation_id(data.sender_uid);
}

}  // namespace

MessagePacketDispatcher::MessagePacketDispatcher(MessageService* message_service)
    : message_service_(message_service) {}

im::message::MessagePacketResponse MessagePacketDispatcher::handle(const im::message::MessagePacketRequest& packet) {
    switch (packet.header().cmd_id()) {
        case im::command::CMD_SEND_MESSAGE:
            return handle_send_message(packet);
        case im::command::CMD_MESSAGE_HISTORY:
            return handle_message_history(packet);
        case im::command::CMD_PULL_MESSAGE:
            return handle_pull_offline(packet);
        default:
            return error_result(packet, im::base::NOT_FOUND, "Unsupported message command");
    }
}

im::message::MessagePacketResponse MessagePacketDispatcher::handle_send_message(const im::message::MessagePacketRequest& packet) {
    if (!message_service_) {
        return error_result(packet, im::base::SERVER_ERROR, "Message service is not initialized");
    }
    if (packet.type_name() == "im.message.SendMessageRequest") {
        return handle_send_message_protobuf(packet);
    }
    if (packet.type_name() == "application/json") {
        return handle_send_message_json(packet);
    }
    return error_result(packet,
                        im::base::INVALID_REQUEST,
                        "Unexpected message packet type: " + packet.type_name());
}

im::message::MessagePacketResponse MessagePacketDispatcher::handle_send_message_protobuf(
        const im::message::MessagePacketRequest& packet) {
    im::message::SendMessageRequest request;
    if (!request.ParseFromString(packet.payload())) {
        return error_result(packet, im::base::INVALID_REQUEST, "Failed to parse SendMessageRequest");
    }

    SendRequest store_request;
    store_request.sender_uid = packet.header().from_uid();
    store_request.receiver_uid = packet.header().to_uid();
    store_request.content = request.body().content();
    store_request.msg_type = to_service_type(request.body().type());
    store_request.now_ms =
            packet.header().timestamp() > 0 ? static_cast<int64_t>(packet.header().timestamp())
                                          : now_ms();

    auto send_result = message_service_->send_text_message(store_request);
    if (!send_result.ok) {
        return error_result(packet, send_error_code(send_result.error_code), send_result.message);
    }

    im::message::SendMessageResponse response;
    response.mutable_base()->set_error_code(im::base::SUCCESS);
    response.mutable_base()->set_error_message("");
    fill_message_body(send_result.data, response.mutable_message());

    im::message::MessagePacketResponse result;
    std::string payload;
    response.SerializeToString(&payload);
    fill_packet_response(&result,
                         packet,
                         im::base::SUCCESS,
                         0,
                         "",
                         "im.message.SendMessageResponse",
                         payload);
    fill_push_event(send_result.data, result.mutable_push_event());
    return result;
}

im::message::MessagePacketResponse MessagePacketDispatcher::handle_send_message_json(
        const im::message::MessagePacketRequest& packet) {
    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        const std::string receiver_uid = body.value("receiver_uid", "");
        const std::string content = body.value("content", "");
        if (packet.header().from_uid().empty() || receiver_uid.empty() || content.empty()) {
            return json_error_result(
                    packet,
                    im::base::PARAM_ERROR,
                    400,
                    "Missing required fields: sender token uid, receiver_uid, content");
        }

        SendRequest store_request;
        store_request.sender_uid = packet.header().from_uid();
        store_request.receiver_uid = receiver_uid;
        store_request.content = content;
        store_request.msg_type = MessageType::TEXT;
        store_request.now_ms =
                packet.header().timestamp() > 0 ? static_cast<int64_t>(packet.header().timestamp())
                                              : now_ms();

        auto send_result = message_service_->send_text_message(store_request);
        if (!send_result.ok) {
            const auto code = send_error_code(send_result.error_code);
            return json_error_result(packet, code, send_http_status(code), send_result.message);
        }

        json response;
        response["message"] = message_data_to_json(send_result.data);

        im::message::MessagePacketResponse result;
        fill_packet_response(&result,
                             packet,
                             im::base::SUCCESS,
                             201,
                             "",
                             "application/json",
                             response.dump());
        fill_push_event(send_result.data, result.mutable_push_event());
        return result;
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::message::MessagePacketResponse MessagePacketDispatcher::handle_message_history(const im::message::MessagePacketRequest& packet) {
    if (!message_service_) {
        return error_result(packet, im::base::SERVER_ERROR, "Message service is not initialized");
    }
    if (packet.type_name() != "application/json") {
        return error_result(packet,
                            im::base::INVALID_REQUEST,
                            "Unexpected message history packet type: " + packet.type_name());
    }

    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        const std::string peer_uid = body.value("peer_uid", "");
        if (packet.header().from_uid().empty() || peer_uid.empty()) {
            return json_error_result(
                    packet,
                    im::base::PARAM_ERROR,
                    400,
                    "Missing required fields: token uid, peer_uid");
        }

        const int64_t before_time = parse_before_time(body);
        const int limit = parse_limit(body, 50, 200);
        const auto messages = message_service_->get_conversation(
                packet.header().from_uid(), peer_uid, before_time, limit);
        return json_messages_result(packet, messages);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, e.what());
    }
}

im::message::MessagePacketResponse MessagePacketDispatcher::handle_pull_offline(const im::message::MessagePacketRequest& packet) {
    if (!message_service_) {
        return error_result(packet, im::base::SERVER_ERROR, "Message service is not initialized");
    }
    if (packet.type_name() != "application/json") {
        return error_result(packet,
                            im::base::INVALID_REQUEST,
                            "Unexpected offline pull packet type: " + packet.type_name());
    }

    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        if (packet.header().from_uid().empty()) {
            return json_error_result(packet,
                                     im::base::PARAM_ERROR,
                                     400,
                                     "Missing required field: token uid");
        }

        const int64_t before_time = parse_before_time(body);
        const int limit = parse_limit(body, 50, 200);
        auto messages = message_service_->pull_offline(packet.header().from_uid(), before_time, limit);
        const int64_t delivered_now = now_ms();
        for (auto& item : messages) {
            if (message_service_->mark_delivered(item.msg_id, delivered_now)) {
                item.status = MessageStatus::DELIVERED;
                item.delivered_time = delivered_now;
            }
        }
        return json_messages_result(packet, messages);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, e.what());
    }
}

}  // namespace im::service::message
