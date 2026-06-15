#include "user_http_controller.hpp"

#include <cctype>
#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

#include "../../common/utils/http_utils.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../auth/multi_platform_auth.hpp"
#include "user_client.hpp"

namespace im::gateway {

using json = nlohmann::json;
using im::service::user::RegisterRequest;
using im::service::user::UpdateProfileRequest;
using im::service::user::UserProfile;
using im::utils::HttpUtils;
using im::utils::LogManager;

namespace {

json profile_to_json(const UserProfile& p) {
    json j;
    j["uid"] = p.uid;
    j["account"] = p.account;
    j["nickname"] = p.nickname;
    j["avatar"] = p.avatar;
    j["gender"] = static_cast<int>(p.gender);
    j["signature"] = p.signature;
    j["create_time"] = p.create_time;
    j["last_login"] = p.last_login;
    j["online"] = p.online;
    return j;
}

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

im::service::user::Gender parse_gender(int gender) {
    switch (gender) {
    case 1:
        return static_cast<im::service::user::Gender>(1);
    case 2:
        return static_cast<im::service::user::Gender>(2);
    case 3:
        return static_cast<im::service::user::Gender>(3);
    case 0:
    default:
        return static_cast<im::service::user::Gender>(0);
    }
}

} // anonymous namespace

UserHttpController::UserHttpController(
    std::shared_ptr<UserClient> user_client,
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr)
    : user_client_(std::move(user_client))
    , auth_mgr_(std::move(auth_mgr))
{
    LogManager::SetLogToFile("user_http_controller", "logs/user_http_controller.log");
    logger_ = LogManager::GetLogger("user_http_controller");
}

void UserHttpController::handle_register(const httplib::Request& req, httplib::Response& res) {
    try {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            HttpUtils::buildResponse(res, 400, "", "Invalid JSON body");
            return;
        }

        std::string account = body.value("account", "");
        std::string password = body.value("password", "");
        std::string nickname = body.value("nickname", "");
        std::string device_id = body.value("device_id", "http-default");
        std::string platform = body.value("platform", "web");

        if (account.empty() || password.empty()) {
            HttpUtils::buildResponse(res, 400, "",
                "Missing required fields: account, password");
            return;
        }

        RegisterRequest reg_req;
        reg_req.account = account;
        reg_req.password = password;
        reg_req.nickname = nickname.empty() ? account : nickname;
        reg_req.now_ms = now_ms();

        auto result = user_client_->register_user(reg_req);

        if (!result.ok) {
            if (result.error_code == "DUPLICATE_ACCOUNT") {
                HttpUtils::buildResponse(res, 409, "",
                    "Account already exists");
            } else {
                HttpUtils::buildResponse(res, 500, "", result.message);
            }
            logger_->warn("Register failed: {} ({})", result.message, result.error_code);
            return;
        }

        auto token_result = auth_mgr_->generate_tokens(
            result.profile.uid, result.profile.account, device_id, platform);

        json response_body;
        response_body["profile"] = profile_to_json(result.profile);
        response_body["access_token"] = token_result.new_access_token;
        response_body["refresh_token"] = token_result.new_refresh_token;

        res.status = 201;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("User registered: {} ({})", result.profile.account, result.profile.uid);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_register: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void UserHttpController::handle_login(const httplib::Request& req, httplib::Response& res) {
    try {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            HttpUtils::buildResponse(res, 400, "", "Invalid JSON body");
            return;
        }

        std::string account = body.value("account", "");
        std::string password = body.value("password", "");
        std::string device_id = body.value("device_id", "http-default");
        std::string platform = body.value("platform", "web");

        if (account.empty() || password.empty()) {
            HttpUtils::buildResponse(res, 400, "",
                "Missing required fields: account, password");
            return;
        }

        auto result = user_client_->login_by_account(account, password, now_ms());

        if (!result.ok) {
            if (result.error_code == "WRONG_PASSWORD" || result.error_code == "INVALID_ACCOUNT") {
                HttpUtils::buildResponse(res, 401, "",
                    "Invalid account or password");
            } else {
                HttpUtils::buildResponse(res, 500, "", result.message);
            }
            logger_->warn("Login failed for {}: {} ({})", account, result.message, result.error_code);
            return;
        }

        auto token_result = auth_mgr_->generate_tokens(
            result.profile.uid, result.profile.account, device_id, platform);

        json response_body;
        response_body["profile"] = profile_to_json(result.profile);
        response_body["access_token"] = token_result.new_access_token;
        response_body["refresh_token"] = token_result.new_refresh_token;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("User logged in: {} ({})", result.profile.account, result.profile.uid);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_login: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void UserHttpController::handle_profile(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            HttpUtils::buildResponse(res, 401, "",
                "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            HttpUtils::buildResponse(res, 401, "",
                "Invalid or expired access token");
            return;
        }

        auto profile = user_client_->get_profile_by_uid(user_info.user_id);
        if (!profile.has_value()) {
            HttpUtils::buildResponse(res, 404, "",
                "User profile not found");
            return;
        }

        json response_body;
        response_body["profile"] = profile_to_json(*profile);

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_profile: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void UserHttpController::handle_update_profile(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            HttpUtils::buildResponse(res, 401, "",
                "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            HttpUtils::buildResponse(res, 401, "",
                "Invalid or expired access token");
            return;
        }

        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            HttpUtils::buildResponse(res, 400, "", "Invalid JSON body");
            return;
        }

        auto current = user_client_->get_profile_by_uid(user_info.user_id);
        if (!current.has_value()) {
            HttpUtils::buildResponse(res, 404, "", "User profile not found");
            return;
        }

        UpdateProfileRequest update;
        update.uid = user_info.user_id;
        update.nickname = body.value("nickname", current->nickname);
        update.avatar = body.value("avatar", current->avatar);
        update.signature = body.value("signature", current->signature);
        update.gender = current->gender;
        if (body.contains("gender") && body["gender"].is_number_integer()) {
            update.gender = parse_gender(body["gender"].get<int>());
        }

        if (update.nickname.empty()) {
            HttpUtils::buildResponse(res, 400, "", "Nickname must not be empty");
            return;
        }

        auto result = user_client_->update_profile(update);
        if (!result.ok) {
            if (result.error_code == "EMPTY_UID" || result.error_code == "EMPTY_NICKNAME" ||
                result.error_code == "PARAM_ERROR") {
                HttpUtils::buildResponse(res, 400, "", result.message);
            } else if (result.error_code == "USER_NOT_FOUND") {
                HttpUtils::buildResponse(res, 404, "", result.message);
            } else {
                HttpUtils::buildResponse(res, 500, "", result.message);
            }
            logger_->warn("Update profile failed for {}: {} ({})",
                          user_info.user_id, result.message, result.error_code);
            return;
        }

        json response_body;
        response_body["profile"] = profile_to_json(result.profile);
        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_update_profile: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void UserHttpController::handle_search_user(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string token = extract_bearer_token(req);
        if (token.empty()) {
            HttpUtils::buildResponse(res, 401, "",
                "Missing or invalid Authorization header");
            return;
        }

        UserTokenInfo user_info;
        if (!auth_mgr_->verify_access_token(token, user_info)) {
            HttpUtils::buildResponse(res, 401, "",
                "Invalid or expired access token");
            return;
        }

        std::string query = req.has_param("q") ? req.get_param_value("q") : "";
        if (query.empty() && req.has_param("keyword")) {
            query = req.get_param_value("keyword");
        }
        if (query.empty()) {
            HttpUtils::buildResponse(res, 400, "",
                "Missing required query parameter: q");
            return;
        }

        const auto profiles = user_client_->search_profiles(query, 20);

        json response_body;
        response_body["users"] = json::array();
        for (const auto& profile : profiles) {
            response_body["users"].push_back(profile_to_json(profile));
        }
        if (!profiles.empty()) {
            response_body["profile"] = profile_to_json(profiles.front());
        }

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_search_user: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

std::string UserHttpController::extract_bearer_token(const httplib::Request& req) const {
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

} // namespace im::gateway
