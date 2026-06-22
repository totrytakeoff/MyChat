#include "group_http_controller.hpp"

#include <cctype>
#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

#include "http_response_builder.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../auth/multi_platform_auth.hpp"
#include "group_client.hpp"
#include "../../services/group/group_service.hpp"

namespace im {
namespace gateway {

using json = nlohmann::json;
using im::service::group::CreateGroupRequest;
using im::service::group::GroupInfoDTO;
using im::service::group::MemberInfoDTO;
using im::service::group::GroupRole;
using im::utils::LogManager;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

json group_info_to_json(const GroupInfoDTO& info) {
    json j;
    j["group_id"] = info.group_id;
    j["name"] = info.name;
    j["creator_uid"] = info.creator_uid;
    j["created_at"] = info.created_at;
    j["member_count"] = info.member_count;
    return j;
}

json member_info_to_json(const MemberInfoDTO& info) {
    json j;
    j["id"] = info.id;
    j["user_uid"] = info.user_uid;
    j["nickname"] = info.nickname;
    j["role"] = static_cast<int>(info.role);
    j["joined_at"] = info.joined_at;
    return j;
}

} // anonymous namespace

GroupHttpController::GroupHttpController(
    std::shared_ptr<GroupClient> group_client,
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr)
    : group_client_(std::move(group_client))
    , auth_mgr_(std::move(auth_mgr))
{
    LogManager::SetLogToFile("group_http_controller", "logs/group_http_controller.log");
    logger_ = LogManager::GetLogger("group_http_controller");
}

void GroupHttpController::handle_create_group(
    const HttpRequest& req, HttpResponse& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            build_http_response(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            build_http_response(res, 401, "", "Invalid or expired access token");
            return;
        }

        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            build_http_response(res, 400, "", "Invalid JSON body");
            return;
        }

        std::string name = body.value("name", "");
        if (name.empty()) {
            build_http_response(res, 400, "", "Missing required field: name");
            return;
        }

        CreateGroupRequest request;
        request.name = name;
        request.creator_uid = user_info.user_id;
        request.now_ms = now_ms();

        auto result = group_client_->create_group(request);

        if (!result.ok) {
            int status = 400;
            if (result.error_code == "CREATOR_NOT_FOUND") status = 404;
            build_http_response(res, status, "", result.message);
            logger_->warn("Create group failed: {} ({})", result.message, result.error_code);
            return;
        }

        json response_body;
        response_body["message"] = result.message;
        response_body["group_id"] = result.group_id;

        res.status = 201;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("Group created: id={}, name={}, creator={}",
            result.group_id, name, user_info.user_id);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_create_group: {}", e.what());
        build_http_response(res, 500, "", "Internal server error");
    }
}

void GroupHttpController::handle_join_group(
    const HttpRequest& req, HttpResponse& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            build_http_response(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            build_http_response(res, 401, "", "Invalid or expired access token");
            return;
        }

        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            build_http_response(res, 400, "", "Invalid JSON body");
            return;
        }

        uint64_t group_id = body.value("group_id", 0ULL);
        if (group_id == 0) {
            build_http_response(res, 400, "", "Missing required field: group_id");
            return;
        }

        auto result = group_client_->join_group(group_id, user_info.user_id, now_ms());

        if (!result.ok) {
            int status = 400;
            if (result.error_code == "GROUP_NOT_FOUND") status = 404;
            else if (result.error_code == "USER_NOT_FOUND") status = 404;
            else if (result.error_code == "ALREADY_MEMBER") status = 409;
            build_http_response(res, status, "", result.message);
            logger_->warn("Join group failed: {} ({})", result.message, result.error_code);
            return;
        }

        json response_body;
        response_body["message"] = result.message;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("User {} joined group {}", user_info.user_id, group_id);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_join_group: {}", e.what());
        build_http_response(res, 500, "", "Internal server error");
    }
}

void GroupHttpController::handle_leave_group(
    const HttpRequest& req, HttpResponse& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            build_http_response(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            build_http_response(res, 401, "", "Invalid or expired access token");
            return;
        }

        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            build_http_response(res, 400, "", "Invalid JSON body");
            return;
        }

        uint64_t group_id = body.value("group_id", 0ULL);
        if (group_id == 0) {
            build_http_response(res, 400, "", "Missing required field: group_id");
            return;
        }

        auto result = group_client_->leave_group(group_id, user_info.user_id);

        if (!result.ok) {
            int status = 400;
            if (result.error_code == "GROUP_NOT_FOUND") status = 404;
            else if (result.error_code == "NOT_MEMBER") status = 403;
            else if (result.error_code == "OWNER_CANNOT_LEAVE") status = 403;
            build_http_response(res, status, "", result.message);
            logger_->warn("Leave group failed: {} ({})", result.message, result.error_code);
            return;
        }

        json response_body;
        response_body["message"] = result.message;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("User {} left group {}", user_info.user_id, group_id);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_leave_group: {}", e.what());
        build_http_response(res, 500, "", "Internal server error");
    }
}

void GroupHttpController::handle_group_info(
    const HttpRequest& req, HttpResponse& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            build_http_response(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            build_http_response(res, 401, "", "Invalid or expired access token");
            return;
        }

        std::string group_id_str = req.get_param_value("group_id");
        if (group_id_str.empty()) {
            build_http_response(res, 400, "", "Missing query parameter: group_id");
            return;
        }

        uint64_t group_id = 0;
        try {
            group_id = std::stoull(group_id_str);
        } catch (...) {
            build_http_response(res, 400, "", "Invalid group_id");
            return;
        }

        auto group = group_client_->get_group_info(group_id);
        if (!group.has_value()) {
            build_http_response(res, 404, "", "Group not found");
            return;
        }

        auto members = group_client_->list_members(group_id, user_info.user_id);
        json response_body;
        response_body["group"] = group_info_to_json(*group);
        response_body["joined"] = !members.empty();
        response_body["members"] = json::array();
        for (const auto& member : members) {
            response_body["members"].push_back(member_info_to_json(member));
        }

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_group_info: {}", e.what());
        build_http_response(res, 500, "", "Internal server error");
    }
}

void GroupHttpController::handle_search_groups(
    const HttpRequest& req, HttpResponse& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            build_http_response(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            build_http_response(res, 401, "", "Invalid or expired access token");
            return;
        }

        std::string query = req.has_param("q") ? req.get_param_value("q") : "";
        if (query.empty() && req.has_param("keyword")) {
            query = req.get_param_value("keyword");
        }
        if (query.empty()) {
            build_http_response(res, 400, "", "Missing required query parameter: q");
            return;
        }

        auto groups = group_client_->search_groups(query, 20);
        auto my_groups = group_client_->list_my_groups(user_info.user_id);

        json groups_array = json::array();
        for (const auto& group : groups) {
            auto item = group_info_to_json(group);
            bool joined = false;
            for (const auto& mine : my_groups) {
                if (mine.group_id == group.group_id) {
                    joined = true;
                    break;
                }
            }
            item["joined"] = joined;
            groups_array.push_back(item);
        }

        json response_body;
        response_body["groups"] = groups_array;
        if (!groups.empty()) {
            response_body["group"] = group_info_to_json(groups.front());
        }

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_search_groups: {}", e.what());
        build_http_response(res, 500, "", "Internal server error");
    }
}

void GroupHttpController::handle_list_groups(
    const HttpRequest& req, HttpResponse& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            build_http_response(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            build_http_response(res, 401, "", "Invalid or expired access token");
            return;
        }

        auto groups = group_client_->list_my_groups(user_info.user_id);

        json groups_array = json::array();
        for (const auto& g : groups) {
            groups_array.push_back(group_info_to_json(g));
        }

        json response_body;
        response_body["groups"] = groups_array;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_list_groups: {}", e.what());
        build_http_response(res, 500, "", "Internal server error");
    }
}

void GroupHttpController::handle_list_members(
    const HttpRequest& req, HttpResponse& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            build_http_response(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            build_http_response(res, 401, "", "Invalid or expired access token");
            return;
        }

        std::string group_id_str = req.get_param_value("group_id");
        if (group_id_str.empty()) {
            build_http_response(res, 400, "", "Missing query parameter: group_id");
            return;
        }

        uint64_t group_id;
        try {
            group_id = std::stoull(group_id_str);
        } catch (...) {
            build_http_response(res, 400, "", "Invalid group_id");
            return;
        }

        if (!group_client_->group_exists(group_id)) {
            build_http_response(res, 404, "", "Group not found");
            return;
        }

        auto members = group_client_->list_members(group_id, user_info.user_id);
        if (members.empty()) {
            build_http_response(res, 403, "", "Not a member of this group");
            return;
        }

        json members_array = json::array();
        for (const auto& m : members) {
            members_array.push_back(member_info_to_json(m));
        }

        json response_body;
        response_body["members"] = members_array;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_list_members: {}", e.what());
        build_http_response(res, 500, "", "Internal server error");
    }
}

std::string GroupHttpController::extract_bearer_token(
    const HttpRequest& req) const
{
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) {
        return "";
    }

    const std::string& auth = it->second;
    const std::string prefix = "Bearer ";
    if (auth.size() <= prefix.size()) {
        return "";
    }

    std::string auth_prefix = auth.substr(0, prefix.size());
    for (auto& c : auth_prefix) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    std::string lower_prefix = prefix;
    for (auto& c : lower_prefix) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (auth_prefix != lower_prefix) {
        return "";
    }

    return auth.substr(prefix.size());
}

} // namespace gateway
} // namespace im
