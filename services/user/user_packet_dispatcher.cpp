#include "user_packet_dispatcher.hpp"

#include <chrono>
#include <exception>
#include <string>

#include <nlohmann/json.hpp>

#include "command.pb.h"
#include "user.hpp"
#include "user_service.hpp"

namespace im::service::user {
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

json profile_to_json(const UserProfile& profile) {
    json body;
    body["uid"] = profile.uid;
    body["account"] = profile.account;
    body["nickname"] = profile.nickname;
    body["avatar"] = profile.avatar;
    body["gender"] = static_cast<int>(profile.gender);
    body["signature"] = profile.signature;
    body["create_time"] = profile.create_time;
    body["last_login"] = profile.last_login;
    body["online"] = profile.online;
    return body;
}

Gender parse_gender(int gender) {
    switch (gender) {
        case 1:
            return Gender::MALE;
        case 2:
            return Gender::FEMALE;
        case 3:
            return Gender::OTHER;
        case 0:
        default:
            return Gender::UNKNOWN;
    }
}

const im::base::IMHeader& packet_header(const im::user::UserPacketRequest& packet) {
    return packet.header();
}

std::string packet_type_name(const im::user::UserPacketRequest& packet) {
    return packet.type_name();
}

std::string packet_payload(const im::user::UserPacketRequest& packet) {
    return packet.payload();
}

void fill_packet_response(im::user::UserPacketResponse* result,
                          const im::user::UserPacketRequest& request,
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

bool packet_ok(const im::user::UserPacketResponse& result) {
    return result.base().error_code() == im::base::SUCCESS;
}

std::string device_from_body_or_header(const json& body, const im::user::UserPacketRequest& packet) {
    const std::string fallback =
            packet.header().device_id().empty() ? "http-default" : packet.header().device_id();
    return body.value("device_id", fallback);
}

std::string platform_from_body_or_header(const json& body, const im::user::UserPacketRequest& packet) {
    const std::string fallback =
            packet.header().platform().empty() || packet.header().platform() == "unknown"
                    ? "web"
                    : packet.header().platform();
    return body.value("platform", fallback);
}

std::string json_error_body(int status, const std::string& message) {
    json body;
    body["code"] = status;
    body["body"] = "";
    body["err_msg"] = message;
    return body.dump();
}

im::user::UserPacketResponse json_error_result(const im::user::UserPacketRequest& request,
                                   im::base::ErrorCode code,
                                   int http_status,
                                   const std::string& message) {
    im::user::UserPacketResponse result;
    fill_packet_response(&result,
                         request,
                         code,
                         http_status,
                         message,
                         "application/json",
                         json_error_body(http_status, message));
    return result;
}

im::user::UserPacketResponse json_success_result(const im::user::UserPacketRequest& request,
                                     int http_status,
                                     const json& payload) {
    im::user::UserPacketResponse result;
    fill_packet_response(&result,
                         request,
                         im::base::SUCCESS,
                         http_status,
                         "",
                         "application/json",
                         payload.dump());
    return result;
}

im::user::UserPacketResponse require_json_packet(const im::user::UserPacketRequest& packet) {
    if (packet.type_name() == "application/json") {
        return im::user::UserPacketResponse{};
    }
    return json_error_result(packet,
                             im::base::INVALID_REQUEST,
                             400,
                             "Unexpected user packet type: " + packet.type_name());
}

im::base::ErrorCode register_error_code(const std::string& code) {
    if (code == "EMPTY_ACCOUNT" || code == "EMPTY_PASSWORD" || code == "PARAM_ERROR") {
        return im::base::PARAM_ERROR;
    }
    if (code == "DUPLICATE_ACCOUNT") {
        return im::base::ALREADY_EXISTS;
    }
    return im::base::SERVER_ERROR;
}

int register_http_status(im::base::ErrorCode code) {
    switch (code) {
        case im::base::PARAM_ERROR:
        case im::base::INVALID_REQUEST:
            return 400;
        case im::base::ALREADY_EXISTS:
            return 409;
        default:
            return 500;
    }
}

im::base::ErrorCode login_error_code(const std::string& code) {
    if (code == "INVALID_ACCOUNT" || code == "WRONG_PASSWORD") {
        return im::base::AUTH_FAILED;
    }
    if (code == "PARAM_ERROR") {
        return im::base::PARAM_ERROR;
    }
    return im::base::SERVER_ERROR;
}

int user_http_status(im::base::ErrorCode code) {
    switch (code) {
        case im::base::AUTH_FAILED:
            return 401;
        case im::base::PARAM_ERROR:
        case im::base::INVALID_REQUEST:
            return 400;
        case im::base::NOT_FOUND:
            return 404;
        case im::base::ALREADY_EXISTS:
            return 409;
        default:
            return 500;
    }
}

im::base::ErrorCode update_error_code(const std::string& code) {
    if (code == "EMPTY_UID" || code == "EMPTY_NICKNAME" || code == "PARAM_ERROR") {
        return im::base::PARAM_ERROR;
    }
    if (code == "USER_NOT_FOUND") {
        return im::base::NOT_FOUND;
    }
    return im::base::SERVER_ERROR;
}

}  // namespace

UserPacketDispatcher::UserPacketDispatcher(UserService* user_service)
    : user_service_(user_service) {}

im::user::UserPacketResponse UserPacketDispatcher::handle(const im::user::UserPacketRequest& packet) {
    switch (packet.header().cmd_id()) {
        case im::command::CMD_REGISTER:
            return handle_register(packet);
        case im::command::CMD_LOGIN:
            return handle_login(packet);
        case im::command::CMD_GET_USER_INFO:
            return handle_profile(packet);
        case im::command::CMD_UPDATE_USER_INFO:
            return handle_update_profile(packet);
        case im::command::CMD_SEARCH_USER:
            return handle_search_user(packet);
        default:
            return json_error_result(packet,
                                     im::base::NOT_FOUND,
                                     404,
                                     "Unsupported user command");
    }
}

im::user::UserPacketResponse UserPacketDispatcher::handle_register(const im::user::UserPacketRequest& packet) {
    if (!user_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "User service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }

    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        const std::string account = body.value("account", "");
        const std::string password = body.value("password", "");
        const std::string nickname = body.value("nickname", "");
        if (account.empty() || password.empty()) {
            return json_error_result(packet,
                                     im::base::PARAM_ERROR,
                                     400,
                                     "Missing required fields: account, password");
        }

        RegisterRequest request;
        request.account = account;
        request.password = password;
        request.nickname = nickname.empty() ? account : nickname;
        request.now_ms = packet.header().timestamp() > 0
                ? static_cast<int64_t>(packet.header().timestamp())
                : now_ms();

        const auto result = user_service_->register_user(request);
        if (!result.ok) {
            const auto code = register_error_code(result.error_code);
            const std::string message = result.error_code == "DUPLICATE_ACCOUNT"
                    ? "Account already exists"
                    : result.message;
            return json_error_result(packet, code, register_http_status(code), message);
        }

        json response;
        response["profile"] = profile_to_json(result.profile);
        auto packet_result = json_success_result(packet, 201, response);
        auto* auth_hint = packet_result.mutable_auth_token_hint();
        auth_hint->set_required(true);
        auth_hint->set_uid(result.profile.uid);
        auth_hint->set_account(result.profile.account);
        auth_hint->set_device_id(device_from_body_or_header(body, packet));
        auth_hint->set_platform(platform_from_body_or_header(body, packet));
        return packet_result;
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::user::UserPacketResponse UserPacketDispatcher::handle_login(const im::user::UserPacketRequest& packet) {
    if (!user_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "User service is not initialized");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }

    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        const std::string account = body.value("account", "");
        const std::string password = body.value("password", "");
        if (account.empty() || password.empty()) {
            return json_error_result(packet,
                                     im::base::PARAM_ERROR,
                                     400,
                                     "Missing required fields: account, password");
        }

        const int64_t request_time = packet.header().timestamp() > 0
                ? static_cast<int64_t>(packet.header().timestamp())
                : now_ms();
        const auto result = user_service_->login_by_account(account, password, request_time);
        if (!result.ok) {
            const auto code = login_error_code(result.error_code);
            const std::string message = code == im::base::AUTH_FAILED
                    ? "Invalid account or password"
                    : result.message;
            return json_error_result(packet, code, user_http_status(code), message);
        }

        json response;
        response["profile"] = profile_to_json(result.profile);
        auto packet_result = json_success_result(packet, 200, response);
        auto* auth_hint = packet_result.mutable_auth_token_hint();
        auth_hint->set_required(true);
        auth_hint->set_uid(result.profile.uid);
        auth_hint->set_account(result.profile.account);
        auth_hint->set_device_id(device_from_body_or_header(body, packet));
        auth_hint->set_platform(platform_from_body_or_header(body, packet));
        return packet_result;
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::user::UserPacketResponse UserPacketDispatcher::handle_profile(const im::user::UserPacketRequest& packet) {
    if (!user_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "User service is not initialized");
    }
    if (packet.header().from_uid().empty()) {
        return json_error_result(packet,
                                 im::base::PARAM_ERROR,
                                 400,
                                 "Missing required field: token uid");
    }

    try {
        const auto profile = user_service_->get_profile_by_uid(packet.header().from_uid());
        if (!profile.has_value()) {
            return json_error_result(packet,
                                     im::base::NOT_FOUND,
                                     404,
                                     "User profile not found");
        }
        json response;
        response["profile"] = profile_to_json(*profile);
        return json_success_result(packet, 200, response);
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::user::UserPacketResponse UserPacketDispatcher::handle_update_profile(const im::user::UserPacketRequest& packet) {
    if (!user_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "User service is not initialized");
    }
    if (packet.header().from_uid().empty()) {
        return json_error_result(packet,
                                 im::base::PARAM_ERROR,
                                 400,
                                 "Missing required field: token uid");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }

    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        const auto current = user_service_->get_profile_by_uid(packet.header().from_uid());
        if (!current.has_value()) {
            return json_error_result(packet,
                                     im::base::NOT_FOUND,
                                     404,
                                     "User profile not found");
        }

        UpdateProfileRequest request;
        request.uid = packet.header().from_uid();
        request.nickname = body.value("nickname", current->nickname);
        request.avatar = body.value("avatar", current->avatar);
        request.signature = body.value("signature", current->signature);
        request.gender = current->gender;
        if (body.contains("gender") && body["gender"].is_number_integer()) {
            request.gender = parse_gender(body["gender"].get<int>());
        }
        if (request.nickname.empty()) {
            return json_error_result(packet,
                                     im::base::PARAM_ERROR,
                                     400,
                                     "Nickname must not be empty");
        }

        const auto result = user_service_->update_profile(request);
        if (!result.ok) {
            const auto code = update_error_code(result.error_code);
            return json_error_result(packet, code, user_http_status(code), result.message);
        }

        json response;
        response["profile"] = profile_to_json(result.profile);
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

im::user::UserPacketResponse UserPacketDispatcher::handle_search_user(const im::user::UserPacketRequest& packet) {
    if (!user_service_) {
        return json_error_result(packet,
                                 im::base::SERVER_ERROR,
                                 500,
                                 "User service is not initialized");
    }
    if (packet.header().from_uid().empty()) {
        return json_error_result(packet,
                                 im::base::PARAM_ERROR,
                                 400,
                                 "Missing required field: token uid");
    }
    auto type_check = require_json_packet(packet);
    if (type_check.http_status() != 0) {
        return type_check;
    }

    try {
        const json body = packet.payload().empty() ? json::object() : json::parse(packet.payload());
        std::string query = body.value("q", "");
        if (query.empty()) {
            query = body.value("keyword", "");
        }
        if (query.empty()) {
            return json_error_result(packet,
                                     im::base::PARAM_ERROR,
                                     400,
                                     "Missing required query parameter: q");
        }

        const auto profiles = user_service_->search_profiles(query, 20);
        json response;
        response["users"] = json::array();
        for (const auto& profile : profiles) {
            response["users"].push_back(profile_to_json(profile));
        }
        if (!profiles.empty()) {
            response["profile"] = profile_to_json(profiles.front());
        }
        return json_success_result(packet, 200, response);
    } catch (const json::exception&) {
        return json_error_result(packet, im::base::INVALID_REQUEST, 400, "Invalid JSON body");
    } catch (const std::exception& e) {
        return json_error_result(packet, im::base::SERVER_ERROR, 500, e.what());
    }
}

}  // namespace im::service::user
