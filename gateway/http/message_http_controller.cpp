#include "message_http_controller.hpp"

#include <cctype>
#include <chrono>
#include <string>

#include <nlohmann/json.hpp>

#include "../../common/utils/http_utils.hpp"
#include "../../common/utils/log_manager.hpp"
#include "../auth/multi_platform_auth.hpp"
#include "../../services/message/message_service.hpp"
#include "../../services/odb/message.hpp"

namespace im::gateway {

using json = nlohmann::json;
using im::service::message::MessageData;
using im::service::message::MessageService;
using im::service::message::SendRequest;
using im::utils::HttpUtils;
using im::utils::LogManager;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

json message_data_to_json(const MessageData& d) {
    json j;
    j["msg_id"] = d.msg_id;
    j["sender_uid"] = d.sender_uid;
    j["receiver_uid"] = d.receiver_uid;
    j["content"] = d.content;
    j["msg_type"] = static_cast<int>(d.msg_type);
    j["status"] = static_cast<int>(d.status);
    j["create_time"] = d.create_time;
    j["delivered_time"] = d.delivered_time;
    j["read_time"] = d.read_time;
    return j;
}

} // anonymous namespace

MessageHttpController::MessageHttpController(
    std::shared_ptr<MessageService> msg_service,
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr)
    : msg_service_(std::move(msg_service))
    , auth_mgr_(std::move(auth_mgr))
{
    LogManager::SetLogToFile("message_http_controller", "logs/message_http_controller.log");
    logger_ = LogManager::GetLogger("message_http_controller");
}

void MessageHttpController::handle_send(const httplib::Request& req, httplib::Response& res) {
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

        std::string receiver_uid = body.value("receiver_uid", "");
        std::string content = body.value("content", "");

        if (receiver_uid.empty() || content.empty()) {
            HttpUtils::buildResponse(res, 400, "",
                "Missing required fields: receiver_uid, content");
            return;
        }

        SendRequest send_req;
        send_req.sender_uid = user_info.user_id;
        send_req.receiver_uid = receiver_uid;
        send_req.content = content;
        send_req.msg_type = im::service::message::MessageType::TEXT;
        send_req.now_ms = now_ms();

        auto result = msg_service_->send_text_message(send_req);

        if (!result.ok) {
            HttpUtils::buildResponse(res, 500, "", result.message);
            logger_->warn("Send message failed: {} ({})", result.message, result.error_code);
            return;
        }

        json response_body;
        response_body["message"] = message_data_to_json(result.data);

        res.status = 201;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("Message sent: {} -> {} (msg_id={})",
            result.data.sender_uid, result.data.receiver_uid, result.data.msg_id);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_send: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void MessageHttpController::handle_history(const httplib::Request& req, httplib::Response& res) {
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

        std::string peer_uid = req.has_param("peer_uid") ? req.get_param_value("peer_uid") : "";
        if (peer_uid.empty()) {
            HttpUtils::buildResponse(res, 400, "",
                "Missing required query parameter: peer_uid");
            return;
        }

        int64_t before_time = INT64_MAX;
        if (req.has_param("before_time")) {
            try {
                before_time = std::stoll(req.get_param_value("before_time"));
            } catch (...) {
                HttpUtils::buildResponse(res, 400, "",
                    "Invalid before_time parameter");
                return;
            }
        }

        int limit = 50;
        if (req.has_param("limit")) {
            try {
                limit = std::stoi(req.get_param_value("limit"));
                if (limit <= 0 || limit > 200) limit = 50;
            } catch (...) {
            }
        }

        auto msgs = msg_service_->get_conversation(
            user_info.user_id, peer_uid, before_time, limit);

        json messages = json::array();
        for (const auto& m : msgs) {
            messages.push_back(message_data_to_json(m));
        }

        json response_body;
        response_body["messages"] = messages;
        response_body["count"] = msgs.size();

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_history: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

void MessageHttpController::handle_offline(const httplib::Request& req, httplib::Response& res) {
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

        int64_t before_time = INT64_MAX;
        if (req.has_param("before_time")) {
            try {
                before_time = std::stoll(req.get_param_value("before_time"));
            } catch (...) {
                HttpUtils::buildResponse(res, 400, "",
                    "Invalid before_time parameter");
                return;
            }
        }

        int limit = 50;
        if (req.has_param("limit")) {
            try {
                limit = std::stoi(req.get_param_value("limit"));
                if (limit <= 0 || limit > 200) limit = 50;
            } catch (...) {
            }
        }

        auto msgs = msg_service_->pull_offline(
            user_info.user_id, before_time, limit);

    // Mark returned messages as delivered and update status in-place
    int64_t delivered_now = now_ms();
    for (auto& m : msgs) {
        msg_service_->mark_delivered(m.msg_id, delivered_now);
        m.status = im::service::message::MessageStatus::DELIVERED;
        m.delivered_time = delivered_now;
    }

    json messages = json::array();
    for (const auto& m : msgs) {
        messages.push_back(message_data_to_json(m));
    }

        json response_body;
        response_body["messages"] = messages;
        response_body["count"] = msgs.size();

        res.status = 200;
        res.set_content(response_body.dump(), "application/json");
        logger_->info("Pulled {} offline messages for user {}", msgs.size(), user_info.user_id);
    } catch (const std::exception& e) {
        logger_->error("Exception in handle_offline: {}", e.what());
        HttpUtils::buildResponse(res, 500, "", "Internal server error");
    }
}

std::string MessageHttpController::extract_bearer_token(const httplib::Request& req) const {
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

} // namespace im::gateway
