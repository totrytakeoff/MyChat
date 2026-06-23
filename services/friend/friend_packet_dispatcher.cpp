#include "friend_packet_dispatcher.hpp"

#include <chrono>
#include <exception>
#include <string>

#include <nlohmann/json.hpp>

#include "command.pb.h"
#include "friend.hpp"
#include "friend_service.hpp"

namespace im::service::friend_ {
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

json friend_info_to_json(const FriendInfoDTO& info) {
    json body;
    body["friend_id"] = info.friend_id;
    body["friend_uid"] = info.friend_uid;
    body["nickname"] = info.nickname;
    body["status"] = static_cast<int>(info.status);
    body["created_at"] = info.created_at;
    return body;
}

std::string json_error_body(int status, const std::string& message) {
    json body;
    body["code"] = status;
    body["body"] = "";
    body["err_msg"] = message;
    return body.dump();
}

void fill_packet_response(im::friend_::FriendPacketResponse* result,
                          const im::friend_::FriendPacketRequest& request,
                          im::base::ErrorCode code,
                          int http_status,
                          const std::string& message,
                          const std::string& payload) {
    result->mutable_base()->set_error_code(code);
    result->mutable_base()->set_error_message(message);
    result->mutable_header()->CopyFrom(response_header(request.header()));
    result->set_type_name("application/json");
    result->set_payload(payload);
    result->set_http_status(http_status);
}

im::friend_::FriendPacketResponse json_error_result(const im::friend_::FriendPacketRequest& request,
                                     im::base::ErrorCode code,
                                     int http_status,
                                     const std::string& message) {
    im::friend_::FriendPacketResponse result;
    fill_packet_response(&result,
                         request,
                         code,
                         http_status,
                         message,
                         json_error_body(http_status, message));
    return result;
}

im::friend_::FriendPacketResponse json_success_result(const im::friend_::FriendPacketRequest& request,
                                       int http_status,
                                       const json& payload) {
    im::friend_::FriendPacketResponse result;
    fill_packet_response(&result,
                         request,
                         im::base::SUCCESS,
                         http_status,
                         "",
                         payload.dump());
    return result;
}

im::base::ErrorCode friend_error_code(const std::string& code) {
    if (code == "EMPTY_UID" ||
        code == "SELF_REQUEST" ||
        code == "NOT_PENDING" ||
        code == "PARAM_ERROR") {
        return im::base::PARAM_ERROR;
    }
    if (code == "TARGET_NOT_FOUND" || code == "NOT_FOUND") {
        return im::base::NOT_FOUND;
    }
    if (code == "ALREADY_EXISTS") {
        return im::base::ALREADY_EXISTS;
    }
    if (code == "FORBIDDEN") {
        return im::base::PERMISSION_DENIED;
    }
    return im::base::SERVER_ERROR;
}

int friend_http_status(im::base::ErrorCode code) {
    switch (code) {
        case im::base::PARAM_ERROR:
        case im::base::INVALID_REQUEST:
            return 400;
        case im::base::NOT_FOUND:
            return 404;
        case im::base::PERMISSION_DENIED:
            return 403;
        case im::base::ALREADY_EXISTS:
            return 409;
        default:
            return 500;
    }
}

im::friend_::FriendPacketResponse service_error_result(const im::friend_::FriendPacketRequest& packet,
                                        const FriendResult& result) {
    const auto code = friend_error_code(result.error_code);
    return json_error_result(packet, code, friend_http_status(code), result.message);
}

im::friend_::FriendPacketResponse require_json_packet(const im::friend_::FriendPacketRequest& packet) {
    if (packet.type_name() == "application/json") {
        return im::friend_::FriendPacketResponse{};
    }
    return json_error_result(packet,
                             im::base::INVALID_REQUEST,
                             400,
                             "Unexpected friend packet type: " + packet.type_name());
}

}  // namespace

FriendPacketDispatcher::FriendPacketDispatcher(FriendService* friend_service)
    : friend_service_(friend_service) {}

im::friend_::FriendPacketResponse FriendPacketDispatcher::handle(const im::friend_::FriendPacketRequest& packet) {
    switch (packet.header().cmd_id()) {
        case im::command::CMD_ADD_FRIEND:
            return handle_add_friend(packet);
        case im::command::CMD_HANDLE_FRIEND_REQUEST:
            return handle_friend_response(packet);
        case im::command::CMD_GET_FRIEND_LIST:
            return handle_friend_list(packet);
        case im::command::CMD_GET_FRIEND_REQUESTS:
            return handle_pending_friends(packet);
        default:
            return json_error_result(packet,
                                     im::base::NOT_FOUND,
                                     404,
                                     "Unsupported friend command");
    }
}

im::friend_::FriendPacketResponse FriendPacketDispatcher::handle_add_friend(const im::friend_::FriendPacketRequest& packet) {
    if (!friend_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "Friend service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }

    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        const std::string target_uid = body.value("target_uid", "");
        if (packet.header().from_uid().empty() || target_uid.empty()) {
            return json_error_result(packet,
                                     im::base::PARAM_ERROR,
                                     400,
                                     "Missing required fields: token uid, target_uid");
        }

        FriendRequest request;
        request.requester_uid = packet.header().from_uid();
        request.target_uid = target_uid;
        request.now_ms = packet.header().timestamp() > 0
                ? static_cast<int64_t>(packet.header().timestamp())
                : now_ms();
        const auto result = friend_service_->send_request(request);
        if (!result.ok) {
            return service_error_result(packet, result);
        }

        json response;
        response["message"] = result.message;
        return json_success_result(packet, 201, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::friend_::FriendPacketResponse FriendPacketDispatcher::handle_friend_response(const im::friend_::FriendPacketRequest& packet) {
    if (!friend_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "Friend service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }

    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        const uint64_t friend_id = body.value("friend_id", 0ULL);
        const bool accept = body.value("accept", false);
        if (packet.header().from_uid().empty() || friend_id == 0) {
            return json_error_result(packet,
                                     im::base::PARAM_ERROR,
                                     400,
                                     "Missing required fields: token uid, friend_id");
        }

        const auto result = friend_service_->respond_to_request(
                friend_id, packet.header().from_uid(), accept);
        if (!result.ok) {
            return service_error_result(packet, result);
        }

        json response;
        response["message"] = result.message;
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::friend_::FriendPacketResponse FriendPacketDispatcher::handle_friend_list(const im::friend_::FriendPacketRequest& packet) {
    if (!friend_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "Friend service is not initialized");
    }
    if (packet.header().from_uid().empty()) {
        return json_error_result(packet,
                                 im::base::PARAM_ERROR,
                                 400,
                                 "Missing required field: token uid");
    }

    try {
        const auto friends = friend_service_->get_friends(packet.header().from_uid());
        json response;
        response["friends"] = json::array();
        for (const auto& item : friends) {
            response["friends"].push_back(friend_info_to_json(item));
        }
        return json_success_result(packet, 200, response);
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::friend_::FriendPacketResponse FriendPacketDispatcher::handle_pending_friends(const im::friend_::FriendPacketRequest& packet) {
    if (!friend_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "Friend service is not initialized");
    }
    if (packet.header().from_uid().empty()) {
        return json_error_result(packet,
                                 im::base::PARAM_ERROR,
                                 400,
                                 "Missing required field: token uid");
    }

    try {
        const auto pending = friend_service_->get_pending_requests(packet.header().from_uid());
        json response;
        response["pending_requests"] = json::array();
        for (const auto& item : pending) {
            response["pending_requests"].push_back(friend_info_to_json(item));
        }
        return json_success_result(packet, 200, response);
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

}  // namespace im::service::friend_
