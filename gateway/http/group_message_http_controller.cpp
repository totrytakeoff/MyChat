#include "group_message_http_controller.hpp"

#include <cctype>
#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

#include "../../common/utils/http_utils.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../auth/multi_platform_auth.hpp"
#include "group_client.hpp"
#include "../../services/push/push_notifier.hpp"
#include "../../services/group/group_message_service.hpp"
#include "../../services/group/group_service.hpp"

namespace im {
namespace gateway {

using json = nlohmann::json;
using im::service::group::GroupMessageDTO;
using im::utils::HttpUtils;
using im::utils::LogManager;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // anonymous namespace

GroupMessageHttpController::GroupMessageHttpController(
    std::shared_ptr<GroupClient> group_client,
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr,
    im::service::push::PushNotifier* push_notifier)
    : group_client_(std::move(group_client))
    , auth_mgr_(std::move(auth_mgr))
    , push_notifier_(push_notifier)
{
    LogManager::SetLogToFile("group_message_http_controller",
                             "logs/group_message_http_controller.log");
    logger_ = LogManager::GetLogger("group_message_http_controller");
}

void GroupMessageHttpController::handle_send_message(
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

        uint64_t group_id = body.value("group_id", 0ULL);
        std::string content = body.value("content", "");

        if (group_id == 0) {
            HttpUtils::buildResponse(res, 400, "", "Missing required field: group_id");
            return;
        }
        if (content.empty()) {
            HttpUtils::buildResponse(res, 400, "", "Missing required field: content");
            return;
        }

        if (!group_client_->group_exists(group_id)) {
            HttpUtils::buildResponse(res, 404, "", "Group not found");
            return;
        }

        // Verify caller is a member and get member list for fanout
        auto members = group_client_->list_members(group_id, user_info.user_id);
        if (members.empty()) {
            HttpUtils::buildResponse(res, 403, "", "Not a member of this group");
            return;
        }

        // Store the message (service also validates, defense in depth)
        int64_t send_time = now_ms();
        auto store_result = group_client_->send_message(
            group_id, user_info.user_id, content, send_time);

        if (!store_result.ok) {
            HttpUtils::buildResponse(res, 500, "", "Failed to send message: " + store_result.message);
            logger_->warn("Send group message failed: {}", store_result.message);
            return;
        }

        // Fanout to all group members
        if (push_notifier_) {
            for (const auto& member : members) {
                if (member.user_uid != user_info.user_id) {
                    im::service::push::PushContext context;
                    context.sender_uid = user_info.user_id;
                    context.conversation_type = "group";
                    context.conversation_id = std::to_string(group_id);
                    push_notifier_->notify_user(member.user_uid,
                                                store_result.msg_id,
                                                content,
                                                context);
                }
            }
        }

        json response_body;
        response_body["message"] = store_result.message;
        response_body["msg_id"] = store_result.msg_id;

        res.status = 201;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("Group message sent: gid={}, sender={}, msg_id={}",
            group_id, user_info.user_id, store_result.msg_id);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_send_message: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void GroupMessageHttpController::handle_get_history(
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

        std::string group_id_str = req.get_param_value("group_id");
        if (group_id_str.empty()) {
            HttpUtils::buildResponse(res, 400, "", "Missing query parameter: group_id");
            return;
        }

        uint64_t group_id;
        try {
            group_id = std::stoull(group_id_str);
        } catch (...) {
            HttpUtils::buildResponse(res, 400, "", "Invalid group_id");
            return;
        }

        if (!group_client_->group_exists(group_id)) {
            HttpUtils::buildResponse(res, 404, "", "Group not found");
            return;
        }

        // Verify caller is a member
        auto members = group_client_->list_members(group_id, user_info.user_id);
        if (members.empty()) {
            HttpUtils::buildResponse(res, 403, "", "Not a member of this group");
            return;
        }

        std::string before_str = req.get_param_value("before");
        int64_t before_time = now_ms();
        if (!before_str.empty()) {
            try {
                before_time = std::stoll(before_str);
            } catch (...) {
                HttpUtils::buildResponse(res, 400, "", "Invalid 'before' parameter");
                return;
            }
        }

        std::string limit_str = req.get_param_value("limit");
        int limit = 50;
        if (!limit_str.empty()) {
            try {
                limit = std::stoi(limit_str);
            } catch (...) {
                HttpUtils::buildResponse(res, 400, "", "Invalid 'limit' parameter");
                return;
            }
        }
        if (limit < 1) limit = 1;
        if (limit > 200) limit = 200;

        auto history = group_client_->get_history(group_id, user_info.user_id,
                                                  before_time, limit);

        json history_array = json::array();
        for (const auto& m : history) {
            json j;
            j["msg_id"] = m.msg_id;
            j["sender_uid"] = m.sender_uid;
            j["sender_nickname"] = m.sender_nickname;
            j["content"] = m.content;
            j["created_at"] = m.created_at;
            history_array.push_back(std::move(j));
        }

        json response_body;
        response_body["messages"] = history_array;

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_get_history: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

std::string GroupMessageHttpController::extract_bearer_token(
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
