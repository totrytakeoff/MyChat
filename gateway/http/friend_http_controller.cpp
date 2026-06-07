#include "friend_http_controller.hpp"

#include <cctype>
#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

#include "../../common/utils/http_utils.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../auth/multi_platform_auth.hpp"
#include "friend_client.hpp"

namespace im {
namespace gateway {

using json = nlohmann::json;
using im::service::friend_::FriendRequest;
using im::service::friend_::FriendInfoDTO;
using im::utils::HttpUtils;
using im::utils::LogManager;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

json friend_info_to_json(const FriendInfoDTO& info) {
    json j;
    j["friend_id"] = info.friend_id;
    j["friend_uid"] = info.friend_uid;
    j["nickname"] = info.nickname;
    j["status"] = static_cast<int>(info.status);
    j["created_at"] = info.created_at;
    return j;
}

} // anonymous namespace

FriendHttpController::FriendHttpController(
    std::shared_ptr<FriendClient> friend_client,
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr)
    : friend_client_(std::move(friend_client))
    , auth_mgr_(std::move(auth_mgr))
{
    LogManager::SetLogToFile("friend_http_controller", "logs/friend_http_controller.log");
    logger_ = LogManager::GetLogger("friend_http_controller");
}

void FriendHttpController::handle_send_request(
    const httplib::Request& req, httplib::Response& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            HttpUtils::buildResponse(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            HttpUtils::buildResponse(res, 401, "", "Invalid or expired access token");
            return;
        }

        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            HttpUtils::buildResponse(res, 400, "", "Invalid JSON body");
            return;
        }

        std::string target_uid = body.value("target_uid", "");
        if (target_uid.empty()) {
            HttpUtils::buildResponse(res, 400, "", "Missing required field: target_uid");
            return;
        }

        FriendRequest request;
        request.requester_uid = user_info.user_id;
        request.target_uid = target_uid;
        request.now_ms = now_ms();

        auto result = friend_client_->send_request(request);

        if (!result.ok) {
            int status = 400;
            if (result.error_code == "ALREADY_EXISTS") status = 409;
            else if (result.error_code == "TARGET_NOT_FOUND") status = 404;
            else if (result.error_code == "SELF_REQUEST") status = 400;
            HttpUtils::buildResponse(res, status, "", result.message);
            logger_->warn("Send friend request failed: {} ({})", result.message, result.error_code);
            return;
        }

        json response_body;
        response_body["message"] = result.message;

        res.status = 201;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("Friend request sent: {} -> {}", user_info.user_id, target_uid);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_send_request: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void FriendHttpController::handle_respond_request(
    const httplib::Request& req, httplib::Response& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            HttpUtils::buildResponse(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            HttpUtils::buildResponse(res, 401, "", "Invalid or expired access token");
            return;
        }

        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            HttpUtils::buildResponse(res, 400, "", "Invalid JSON body");
            return;
        }

        uint64_t friend_id = body.value("friend_id", 0ULL);
        bool accept = body.value("accept", false);

        if (friend_id == 0) {
            HttpUtils::buildResponse(res, 400, "", "Missing required field: friend_id");
            return;
        }

        auto result = friend_client_->respond_to_request(friend_id, user_info.user_id, accept);

        if (!result.ok) {
            int status = 400;
            if (result.error_code == "NOT_FOUND") status = 404;
            else if (result.error_code == "FORBIDDEN") status = 403;
            HttpUtils::buildResponse(res, status, "", result.message);
            logger_->warn("Respond to friend request failed: {} ({})", result.message, result.error_code);
            return;
        }

        json response_body;
        response_body["message"] = result.message;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("Friend request {}: friend_id={}, user={}",
            accept ? "accepted" : "rejected", friend_id, user_info.user_id);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_respond_request: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void FriendHttpController::handle_list_friends(
    const httplib::Request& req, httplib::Response& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            HttpUtils::buildResponse(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            HttpUtils::buildResponse(res, 401, "", "Invalid or expired access token");
            return;
        }

        auto friends = friend_client_->get_friends(user_info.user_id);

        json friends_array = json::array();
        for (const auto& f : friends) {
            friends_array.push_back(friend_info_to_json(f));
        }

        json response_body;
        response_body["friends"] = friends_array;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_list_friends: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void FriendHttpController::handle_pending_requests(
    const httplib::Request& req, httplib::Response& res)
{
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            HttpUtils::buildResponse(res, 401, "", "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            HttpUtils::buildResponse(res, 401, "", "Invalid or expired access token");
            return;
        }

        auto pending = friend_client_->get_pending_requests(user_info.user_id);

        json pending_array = json::array();
        for (const auto& p : pending) {
            pending_array.push_back(friend_info_to_json(p));
        }

        json response_body;
        response_body["pending_requests"] = pending_array;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_pending_requests: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

std::string FriendHttpController::extract_bearer_token(
    const httplib::Request& req) const
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

    // Case-insensitive prefix check
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
