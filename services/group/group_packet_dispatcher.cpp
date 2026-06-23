#include "group_packet_dispatcher.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "command.pb.h"
#include "group_message_service.hpp"
#include "group_service.hpp"

namespace im::service::group {
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

json group_info_to_json(const GroupInfoDTO& info) {
    json body;
    body["group_id"] = info.group_id;
    body["name"] = info.name;
    body["creator_uid"] = info.creator_uid;
    body["created_at"] = info.created_at;
    body["member_count"] = info.member_count;
    return body;
}

json member_info_to_json(const MemberInfoDTO& info) {
    json body;
    body["id"] = info.id;
    body["user_uid"] = info.user_uid;
    body["nickname"] = info.nickname;
    body["role"] = static_cast<int>(info.role);
    body["joined_at"] = info.joined_at;
    return body;
}

json message_to_json(const GroupMessageDTO& message) {
    json body;
    body["msg_id"] = message.msg_id;
    body["sender_uid"] = message.sender_uid;
    body["sender_nickname"] = message.sender_nickname;
    body["content"] = message.content;
    body["created_at"] = message.created_at;
    return body;
}

std::string json_error_body(int status, const std::string& message) {
    json body;
    body["code"] = status;
    body["body"] = "";
    body["err_msg"] = message;
    return body.dump();
}

void fill_packet_response(im::group::GroupPacketResponse* result,
                          const im::group::GroupPacketRequest& request,
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

im::group::GroupPacketResponse json_error_result(const im::group::GroupPacketRequest& request,
                                    im::base::ErrorCode code,
                                    int http_status,
                                    const std::string& message) {
    im::group::GroupPacketResponse result;
    fill_packet_response(&result,
                         request,
                         code,
                         http_status,
                         message,
                         json_error_body(http_status, message));
    return result;
}

im::group::GroupPacketResponse json_success_result(const im::group::GroupPacketRequest& request,
                                      int http_status,
                                      const json& payload) {
    im::group::GroupPacketResponse result;
    fill_packet_response(&result,
                         request,
                         im::base::SUCCESS,
                         http_status,
                         "",
                         payload.dump());
    return result;
}

im::group::GroupPacketResponse require_json_packet(const im::group::GroupPacketRequest& packet) {
    if (packet.type_name() == "application/json") {
        return im::group::GroupPacketResponse{};
    }
    return json_error_result(packet,
                             im::base::INVALID_REQUEST,
                             400,
                             "Unexpected group packet type: " + packet.type_name());
}

im::base::ErrorCode group_error_code(const std::string& error_code) {
    if (error_code == "EMPTY_NAME" ||
        error_code == "EMPTY_CREATOR" ||
        error_code == "EMPTY_UID" ||
        error_code == "EMPTY_SENDER" ||
        error_code == "INVALID_GROUP") {
        return im::base::PARAM_ERROR;
    }
    if (error_code == "GROUP_NOT_FOUND" ||
        error_code == "CREATOR_NOT_FOUND" ||
        error_code == "USER_NOT_FOUND") {
        return im::base::NOT_FOUND;
    }
    if (error_code == "ALREADY_MEMBER") {
        return im::base::ALREADY_EXISTS;
    }
    if (error_code == "NOT_MEMBER" ||
        error_code == "OWNER_CANNOT_LEAVE" ||
        error_code == "FORBIDDEN") {
        return im::base::PERMISSION_DENIED;
    }
    return im::base::SERVER_ERROR;
}

int group_http_status(im::base::ErrorCode code) {
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

json parse_json_payload(const im::group::GroupPacketRequest& packet) {
    return packet.payload().empty() ? json::object() : json::parse(packet.payload());
}

std::string string_value(const json& body, const std::string& key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        return "";
    }
    if (body.at(key).is_string()) {
        return body.at(key).get<std::string>();
    }
    if (body.at(key).is_number_unsigned()) {
        return std::to_string(body.at(key).get<uint64_t>());
    }
    if (body.at(key).is_number_integer()) {
        return std::to_string(body.at(key).get<int64_t>());
    }
    return "";
}

uint64_t uint_value(const json& body, const std::string& key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        return 0;
    }
    if (body.at(key).is_number_unsigned()) {
        return body.at(key).get<uint64_t>();
    }
    if (body.at(key).is_number_integer()) {
        const auto value = body.at(key).get<int64_t>();
        return value > 0 ? static_cast<uint64_t>(value) : 0;
    }
    if (body.at(key).is_string()) {
        const auto value = body.at(key).get<std::string>();
        if (value.empty()) {
            return 0;
        }
        return static_cast<uint64_t>(std::stoull(value));
    }
    return 0;
}

int parse_limit(const json& body, int default_value, int max_value) {
    int limit = default_value;
    if (body.contains("limit")) {
        if (body["limit"].is_number_integer()) {
            limit = body["limit"].get<int>();
        } else if (body["limit"].is_string()) {
            const auto value = body["limit"].get<std::string>();
            if (!value.empty()) {
                limit = std::stoi(value);
            }
        }
    }
    if (limit <= 0 || limit > max_value) {
        return default_value;
    }
    return limit;
}

bool parse_before_time(const json& body, int64_t* out) {
    if (!out) {
        return false;
    }
    *out = now_ms();
    const char* keys[] = {"before", "before_time", "since"};
    for (const auto* key : keys) {
        if (!body.contains(key) || body.at(key).is_null()) {
            continue;
        }
        if (body.at(key).is_number_integer()) {
            *out = body.at(key).get<int64_t>();
            return true;
        }
        if (body.at(key).is_string()) {
            const auto value = body.at(key).get<std::string>();
            if (value.empty()) {
                return true;
            }
            *out = std::stoll(value);
            return true;
        }
        return false;
    }
    return true;
}

bool require_actor_uid(const im::group::GroupPacketRequest& packet, im::group::GroupPacketResponse* result) {
    if (!packet.header().from_uid().empty()) {
        return true;
    }
    if (result) {
        *result = json_error_result(packet,
                                    im::base::PARAM_ERROR,
                                    400,
                                    "Missing required field: token uid");
    }
    return false;
}

void fill_push_events(const std::vector<MemberInfoDTO>& members,
                      const std::string& sender_uid,
                      uint64_t group_id,
                      uint64_t msg_id,
                      const std::string& content,
                      im::group::GroupPacketResponse* response) {
    if (!response) {
        return;
    }
    for (const auto& member : members) {
        if (member.user_uid == sender_uid) {
            continue;
        }
        auto* event = response->add_push_events();
        event->set_enabled(true);
        event->set_receiver_uid(member.user_uid);
        event->set_msg_id(msg_id);
        event->set_content(content);
        event->set_sender_uid(sender_uid);
        event->set_conversation_type("group");
        event->set_conversation_id(std::to_string(group_id));
    }
}

}  // namespace

GroupPacketDispatcher::GroupPacketDispatcher(GroupService* group_service,
                                             GroupMessageService* group_message_service)
    : group_service_(group_service)
    , group_message_service_(group_message_service) {}

im::group::GroupPacketResponse GroupPacketDispatcher::handle(const im::group::GroupPacketRequest& packet) {
    switch (packet.header().cmd_id()) {
        case im::command::CMD_CREATE_GROUP:
            return handle_create_group(packet);
        case im::command::CMD_SEARCH_GROUP:
            return handle_search_group(packet);
        case im::command::CMD_GET_GROUP_INFO:
            return handle_group_info(packet);
        case im::command::CMD_GET_GROUP_LIST:
            return handle_group_list(packet);
        case im::command::CMD_APPLY_JOIN_GROUP:
            return handle_join_group(packet);
        case im::command::CMD_QUIT_GROUP:
            return handle_leave_group(packet);
        case im::command::CMD_GET_GROUP_MEMBERS:
            return handle_group_members(packet);
        case im::command::CMD_SEND_GROUP_MESSAGE:
            return handle_send_group_message(packet);
        case im::command::CMD_GET_GROUP_MESSAGES:
            return handle_group_history(packet);
        default:
            return json_error_result(packet,
                                     im::base::NOT_FOUND,
                                     404,
                                     "Unsupported group command");
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_create_group(const im::group::GroupPacketRequest& packet) {
    if (!group_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto body = parse_json_payload(packet);
        const std::string name = string_value(body, "name");
        if (name.empty()) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing required field: name");
        }

        CreateGroupRequest request;
        request.name = name;
        request.creator_uid = packet.header().from_uid();
        request.now_ms = packet.header().timestamp() > 0
                ? static_cast<int64_t>(packet.header().timestamp())
                : now_ms();

        const auto result = group_service_->create_group(request);
        if (!result.ok) {
            const auto code = group_error_code(result.error_code);
            return json_error_result(packet, code, group_http_status(code), result.message);
        }

        json response;
        response["message"] = result.message;
        response["group_id"] = result.group_id;
        return json_success_result(packet, 201, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_search_group(const im::group::GroupPacketRequest& packet) {
    if (!group_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto body = parse_json_payload(packet);
        std::string query = string_value(body, "q");
        if (query.empty()) {
            query = string_value(body, "keyword");
        }
        if (query.empty()) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing required query parameter: q");
        }

        const auto groups = group_service_->search_groups(query, 20);
        const auto my_groups = group_service_->list_my_groups(packet.header().from_uid());
        json response;
        response["groups"] = json::array();
        for (const auto& group : groups) {
            auto item = group_info_to_json(group);
            item["joined"] = std::any_of(my_groups.begin(), my_groups.end(),
                                         [&group](const auto& mine) {
                                             return mine.group_id == group.group_id;
                                         });
            response["groups"].push_back(std::move(item));
        }
        if (!groups.empty()) {
            response["group"] = group_info_to_json(groups.front());
        }
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_group_info(const im::group::GroupPacketRequest& packet) {
    if (!group_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto body = parse_json_payload(packet);
        const uint64_t group_id = uint_value(body, "group_id");
        if (group_id == 0) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing query parameter: group_id");
        }

        const auto group = group_service_->get_group_info(group_id);
        if (!group.has_value()) {
            return json_error_result(packet, im::base::NOT_FOUND, 404, "Group not found");
        }

        const auto members = group_service_->list_members(group_id, packet.header().from_uid());
        json response;
        response["group"] = group_info_to_json(*group);
        response["joined"] = !members.empty();
        response["members"] = json::array();
        for (const auto& member : members) {
            response["members"].push_back(member_info_to_json(member));
        }
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception&) {
        return json_error_result(packet, im::base::PARAM_ERROR, 400, "Invalid group_id");
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_group_list(const im::group::GroupPacketRequest& packet) {
    if (!group_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group service is not initialized");
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto groups = group_service_->list_my_groups(packet.header().from_uid());
        json response;
        response["groups"] = json::array();
        for (const auto& group : groups) {
            response["groups"].push_back(group_info_to_json(group));
        }
        return json_success_result(packet, 200, response);
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_join_group(const im::group::GroupPacketRequest& packet) {
    if (!group_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto body = parse_json_payload(packet);
        const uint64_t group_id = uint_value(body, "group_id");
        if (group_id == 0) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing required field: group_id");
        }

        const auto result = group_service_->join_group(
                group_id,
                packet.header().from_uid(),
                packet.header().timestamp() > 0
                        ? static_cast<int64_t>(packet.header().timestamp())
                        : now_ms());
        if (!result.ok) {
            const auto code = group_error_code(result.error_code);
            return json_error_result(packet, code, group_http_status(code), result.message);
        }

        json response;
        response["message"] = result.message;
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception&) {
        return json_error_result(packet, im::base::PARAM_ERROR, 400, "Invalid group_id");
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_leave_group(const im::group::GroupPacketRequest& packet) {
    if (!group_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto body = parse_json_payload(packet);
        const uint64_t group_id = uint_value(body, "group_id");
        if (group_id == 0) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing required field: group_id");
        }

        const auto result = group_service_->leave_group(group_id, packet.header().from_uid());
        if (!result.ok) {
            const auto code = group_error_code(result.error_code);
            return json_error_result(packet, code, group_http_status(code), result.message);
        }

        json response;
        response["message"] = result.message;
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception&) {
        return json_error_result(packet, im::base::PARAM_ERROR, 400, "Invalid group_id");
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_group_members(const im::group::GroupPacketRequest& packet) {
    if (!group_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto body = parse_json_payload(packet);
        const uint64_t group_id = uint_value(body, "group_id");
        if (group_id == 0) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing query parameter: group_id");
        }
        if (!group_service_->group_exists(group_id)) {
            return json_error_result(packet, im::base::NOT_FOUND, 404, "Group not found");
        }

        const auto members = group_service_->list_members(group_id, packet.header().from_uid());
        if (members.empty()) {
            return json_error_result(packet,
                                     im::base::PERMISSION_DENIED,
                                     403,
                                     "Not a member of this group");
        }

        json response;
        response["members"] = json::array();
        for (const auto& member : members) {
            response["members"].push_back(member_info_to_json(member));
        }
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception&) {
        return json_error_result(packet, im::base::PARAM_ERROR, 400, "Invalid group_id");
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_send_group_message(const im::group::GroupPacketRequest& packet) {
    if (!group_service_ || !group_message_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group message service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto body = parse_json_payload(packet);
        const uint64_t group_id = uint_value(body, "group_id");
        const std::string content = string_value(body, "content");
        if (group_id == 0) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing required field: group_id");
        }
        if (content.empty()) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing required field: content");
        }
        if (!group_service_->group_exists(group_id)) {
            return json_error_result(packet, im::base::NOT_FOUND, 404, "Group not found");
        }

        const auto members = group_service_->list_members(group_id, packet.header().from_uid());
        if (members.empty()) {
            return json_error_result(packet,
                                     im::base::PERMISSION_DENIED,
                                     403,
                                     "Not a member of this group");
        }

        const auto result = group_message_service_->send_message(
                group_id,
                packet.header().from_uid(),
                content,
                packet.header().timestamp() > 0
                        ? static_cast<int64_t>(packet.header().timestamp())
                        : now_ms());
        if (!result.ok) {
            const auto code = group_error_code(result.error_code);
            return json_error_result(packet,
                                     code,
                                     group_http_status(code),
                                     "Failed to send message: " + result.message);
        }

        json response;
        response["message"] = result.message;
        response["msg_id"] = result.msg_id;

        auto packet_result = json_success_result(packet, 201, response);
        fill_push_events(members,
                         packet.header().from_uid(),
                         group_id,
                         result.msg_id,
                         content,
                         &packet_result);
        return packet_result;
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception&) {
        return json_error_result(packet, im::base::PARAM_ERROR, 400, "Invalid group_id");
    }
}

im::group::GroupPacketResponse GroupPacketDispatcher::handle_group_history(const im::group::GroupPacketRequest& packet) {
    if (!group_service_ || !group_message_service_) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500,
                                 "Group message service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }
    im::group::GroupPacketResponse actor_error;
    if (!require_actor_uid(packet, &actor_error)) {
        return actor_error;
    }

    try {
        const auto body = parse_json_payload(packet);
        const uint64_t group_id = uint_value(body, "group_id");
        if (group_id == 0) {
            return json_error_result(packet, im::base::PARAM_ERROR, 400,
                                     "Missing query parameter: group_id");
        }
        if (!group_service_->group_exists(group_id)) {
            return json_error_result(packet, im::base::NOT_FOUND, 404, "Group not found");
        }
        if (group_service_->list_members(group_id, packet.header().from_uid()).empty()) {
            return json_error_result(packet,
                                     im::base::PERMISSION_DENIED,
                                     403,
                                     "Not a member of this group");
        }

        int64_t before_time = now_ms();
        if (!parse_before_time(body, &before_time)) {
            return json_error_result(packet,
                                     im::base::PARAM_ERROR,
                                     400,
                                     "Invalid 'before' parameter");
        }
        const int limit = parse_limit(body, 50, 200);
        const auto history = group_message_service_->get_history(group_id, before_time, limit);

        json response;
        response["messages"] = json::array();
        for (const auto& item : history) {
            response["messages"].push_back(message_to_json(item));
        }
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception&) {
        return json_error_result(packet, im::base::PARAM_ERROR, 400, "Invalid group_id");
    }
}

}  // namespace im::service::group
