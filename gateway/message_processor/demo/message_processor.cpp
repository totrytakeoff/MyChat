#include "message_processor.hpp"
#include "../../common/proto/command.pb.h"
#include "../../common/proto/message.pb.h"
#include "../../common/proto/user.pb.h"
#include "../../common/proto/friend.pb.h"
#include "../../common/proto/group.pb.h"
#include "../../common/proto/push.pb.h"

#include <google/protobuf/util/json_util.h>
#include <regex>
#include <unordered_map>

namespace im {
namespace gateway {

// ================================
// InternalMessage 实现
// ================================

uint32_t InternalMessage::get_cmd_id() const {
    return header_.cmd_id();
}

const std::string& InternalMessage::get_from_uid() const {
    return header_.from_uid();
}

const std::string& InternalMessage::get_to_uid() const {
    return header_.to_uid();
}

const std::string& InternalMessage::get_token() const {
    return header_.token();
}

const std::string& InternalMessage::get_device_id() const {
    return header_.device_id();
}

const std::string& InternalMessage::get_platform() const {
    return header_.platform();
}

const google::protobuf::Message* InternalMessage::get_message() const {
    return protobuf_message_.get();
}

const std::string& InternalMessage::get_json_body() const {
    return json_body_;
}

const InternalMessage::MessageContext& InternalMessage::get_context() const {
    return context_;
}

// InternalMessage的setter方法实现
void InternalMessage::set_header(const im::base::IMHeader& header) {
    header_ = header;
}

void InternalMessage::set_header(im::base::IMHeader&& header) {
    header_ = std::move(header);
}

void InternalMessage::set_protobuf_message(std::unique_ptr<google::protobuf::Message> message) {
    protobuf_message_ = std::move(message);
}

void InternalMessage::set_json_body(const std::string& json_body) {
    json_body_ = json_body;
}

void InternalMessage::set_json_body(std::string&& json_body) {
    json_body_ = std::move(json_body);
}

void InternalMessage::set_context(const MessageContext& context) {
    context_ = context;
}

void InternalMessage::set_context(MessageContext&& context) {
    context_ = std::move(context);
}

// ================================
// URL路径到命令ID的映射表
// ================================

static const std::unordered_map<std::string, uint32_t> URL_CMD_MAPPING = {
    // 用户相关
    {"/api/v1/auth/login",          static_cast<uint32_t>(im::command::CMD_LOGIN)},
    {"/api/v1/auth/logout",         static_cast<uint32_t>(im::command::CMD_LOGOUT)},
    {"/api/v1/auth/register",       static_cast<uint32_t>(im::command::CMD_REGISTER)},
    {"/api/v1/user/info",           static_cast<uint32_t>(im::command::CMD_GET_USER_INFO)},
    {"/api/v1/user/update",         static_cast<uint32_t>(im::command::CMD_UPDATE_USER_INFO)},
    
    // 消息相关
    {"/api/v1/message/send",        static_cast<uint32_t>(im::command::CMD_SEND_MESSAGE)},
    {"/api/v1/message/pull",        static_cast<uint32_t>(im::command::CMD_PULL_MESSAGE)},
    {"/api/v1/message/history",     static_cast<uint32_t>(im::command::CMD_MESSAGE_HISTORY)},
    {"/api/v1/message/delete",      static_cast<uint32_t>(im::command::CMD_DELETE_MESSAGE)},
    {"/api/v1/message/recall",      static_cast<uint32_t>(im::command::CMD_RECALL_MESSAGE)},
    {"/api/v1/message/ack",         static_cast<uint32_t>(im::command::CMD_MESSAGE_ACK)},
    
    // 好友相关
    {"/api/v1/friend/list",         static_cast<uint32_t>(im::command::CMD_GET_FRIEND_LIST)},
    {"/api/v1/friend/add",          static_cast<uint32_t>(im::command::CMD_ADD_FRIEND)},
    {"/api/v1/friend/remove",       static_cast<uint32_t>(im::command::CMD_REMOVE_FRIEND)},
    {"/api/v1/friend/requests",     static_cast<uint32_t>(im::command::CMD_GET_FRIEND_REQUESTS)},
    {"/api/v1/friend/handle",       static_cast<uint32_t>(im::command::CMD_HANDLE_FRIEND_REQUEST)},
    {"/api/v1/user/search",         static_cast<uint32_t>(im::command::CMD_SEARCH_USER)},
    
    // 群组相关
    {"/api/v1/group/create",        static_cast<uint32_t>(im::command::CMD_CREATE_GROUP)},
    {"/api/v1/group/info",          static_cast<uint32_t>(im::command::CMD_GET_GROUP_INFO)},
    {"/api/v1/group/list",          static_cast<uint32_t>(im::command::CMD_GET_GROUP_LIST)},
    {"/api/v1/group/update",        static_cast<uint32_t>(im::command::CMD_MODIFY_GROUP_INFO)},
    {"/api/v1/group/invite",        static_cast<uint32_t>(im::command::CMD_INVITE_MEMBER)},
    {"/api/v1/group/kick",          static_cast<uint32_t>(im::command::CMD_KICK_MEMBER)},
    {"/api/v1/group/apply",         static_cast<uint32_t>(im::command::CMD_APPLY_JOIN_GROUP)},
    {"/api/v1/group/quit",          static_cast<uint32_t>(im::command::CMD_QUIT_GROUP)},
    {"/api/v1/group/members",       static_cast<uint32_t>(im::command::CMD_GET_GROUP_MEMBERS)},
    {"/api/v1/group/messages",      static_cast<uint32_t>(im::command::CMD_GET_GROUP_MESSAGES)},
    {"/api/v1/group/transfer",      static_cast<uint32_t>(im::command::CMD_TRANSFER_GROUP_OWNER)},
    {"/api/v1/group/admin",         static_cast<uint32_t>(im::command::CMD_SET_GROUP_ADMIN)},
};

// ================================
// HttpMessageParser 实现
// ================================

HttpMessageParser::HttpParseResult HttpMessageParser::parse(const httplib::Request& req) {
    HttpParseResult result;
    result.is_valid = false;
    
    // 验证HTTP请求格式
    if (!validate_http_request(req)) {
        result.error_message = "Invalid HTTP request format";
        return result;
    }
    
    // 从URL路径提取命令ID
    result.cmd_id = extract_cmd_from_path(req.path);
    if (result.cmd_id == 0) {
        result.error_message = "Unknown API path: " + req.path;
        return result;
    }
    
    // 提取认证信息
    extract_auth_info(req, result);
    
    // 获取请求体（JSON）
    result.body_json = req.body;
    
    result.is_valid = true;
    return result;
}

uint32_t HttpMessageParser::extract_cmd_from_path(const std::string& path) {
    auto it = URL_CMD_MAPPING.find(path);
    if (it != URL_CMD_MAPPING.end()) {
        return it->second;
    }
    
    // 如果直接匹配失败，尝试处理带参数的路径
    // 例如 /api/v1/user/info/123 -> /api/v1/user/info
    std::regex path_regex(R"(^(/api/v1/[^/]+/[^/]+)(/.*)?$)");
    std::smatch matches;
    
    if (std::regex_match(path, matches, path_regex)) {
        std::string base_path = matches[1].str();
        auto base_it = URL_CMD_MAPPING.find(base_path);
        if (base_it != URL_CMD_MAPPING.end()) {
            return base_it->second;
        }
    }
    
    return 0;  // 未知命令
}

bool HttpMessageParser::validate_http_request(const httplib::Request& req) {
    // 检查是否是支持的HTTP方法
    if (req.method != "GET" && req.method != "POST" && req.method != "PUT" && req.method != "DELETE") {
        return false;
    }
    
    // 检查路径是否以/api/v1开头
    if (req.path.find("/api/v1/") != 0) {
        return false;
    }
    
    // 对于POST和PUT请求，检查Content-Type
    if (req.method == "POST" || req.method == "PUT") {
        auto content_type_it = req.headers.find("Content-Type");
        if (content_type_it == req.headers.end()) {
            return false;
        }
        
        const std::string& content_type = content_type_it->second;
        if (content_type.find("application/json") == std::string::npos) {
            return false;
        }
    }
    
    return true;
}

void HttpMessageParser::extract_auth_info(const httplib::Request& req, HttpParseResult& result) {
    // 从Header中提取token
    auto auth_it = req.headers.find("Authorization");
    if (auth_it != req.headers.end()) {
        const std::string& auth_value = auth_it->second;
        // 支持 "Bearer token" 格式
        if (auth_value.find("Bearer ") == 0) {
            result.token = auth_value.substr(7);  // 去掉 "Bearer "
        } else {
            result.token = auth_value;
        }
    }
    
    // 从Header或Query参数中提取device_id
    auto device_id_header = req.headers.find("X-Device-ID");
    if (device_id_header != req.headers.end()) {
        result.device_id = device_id_header->second;
    } else {
        auto device_id_param = req.params.find("device_id");
        if (device_id_param != req.params.end()) {
            result.device_id = device_id_param->second;
        }
    }
    
    // 从Header或Query参数中提取platform
    auto platform_header = req.headers.find("X-Platform");
    if (platform_header != req.headers.end()) {
        result.platform = platform_header->second;
    } else {
        auto platform_param = req.params.find("platform");
        if (platform_param != req.params.end()) {
            result.platform = platform_param->second;
        }
    }
}

// ================================
// WebSocketMessageParser 实现
// ================================

WebSocketMessageParser::WSParseResult WebSocketMessageParser::parse(const std::string& raw_message) {
    WSParseResult result;
    result.is_valid = false;
    
    try {
        // 使用ProtobufCodec解码消息
        im::base::IMHeader header;
        
        // 首先尝试创建一个临时的消息对象来解码
        // 先解析header获取cmd_id，然后再创建正确的消息类型
        
        // 临时解码到一个基础消息对象
        im::base::BaseResponse temp_message;
        if (!codec_.decode(raw_message, header, temp_message)) {
            result.error_message = "Failed to decode protobuf message";
            return result;
        }
        
        // 根据cmd_id创建正确的消息对象
        result.message = create_message_by_cmd(header.cmd_id());
        if (!result.message) {
            result.error_message = "Unsupported command ID: " + std::to_string(header.cmd_id());
            return result;
        }
        
        // 重新解码到正确的消息类型
        if (!codec_.decode(raw_message, header, *result.message)) {
            result.error_message = "Failed to decode to specific message type";
            return result;
        }
        
        // 验证消息完整性
        if (!validate_message(header, *result.message)) {
            result.error_message = "Message validation failed";
            return result;
        }
        
        result.header = std::move(header);
        result.is_valid = true;
        
    } catch (const std::exception& e) {
        result.error_message = "Exception during parsing: " + std::string(e.what());
    }
    
    return result;
}

bool WebSocketMessageParser::validate_message(const im::base::IMHeader& header, 
                                             const google::protobuf::Message& message) {
    // 验证必需字段
    if (header.cmd_id() == 0) {
        return false;
    }
    
    // 验证版本号
    if (header.version().empty()) {
        return false;
    }
    
    // 验证时间戳（应该是合理的时间范围）
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto header_ts = static_cast<int64_t>(header.timestamp());
    
    // 允许5分钟的时间差
    const int64_t MAX_TIME_DIFF = 5 * 60 * 1000;  // 5分钟（毫秒）
    if (std::abs(now_ms - header_ts) > MAX_TIME_DIFF) {
        return false;
    }
    
    // 验证消息是否完全初始化
    if (!message.IsInitialized()) {
        return false;
    }
    
    return true;
}

std::unique_ptr<google::protobuf::Message> WebSocketMessageParser::create_message_by_cmd(uint32_t cmd_id) {
    using namespace im::command;
    
    switch (static_cast<CommandID>(cmd_id)) {
        // 用户相关消息
        case CMD_LOGIN:
        case CMD_REGISTER:
            return std::make_unique<im::user::LoginRequest>();
            
        case CMD_LOGOUT:
            return std::make_unique<im::user::LogoutRequest>();
            
        case CMD_GET_USER_INFO:
            return std::make_unique<im::user::GetUserInfoRequest>();
            
        case CMD_UPDATE_USER_INFO:
            return std::make_unique<im::user::UpdateUserInfoRequest>();
            
        case CMD_USER_ONLINE:
        case CMD_USER_OFFLINE:
            return std::make_unique<im::user::UserStatusNotify>();
            
        // 消息相关
        case CMD_SEND_MESSAGE:
            return std::make_unique<im::message::SendMessageRequest>();
            
        case CMD_PULL_MESSAGE:
            return std::make_unique<im::message::PullMessageRequest>();
            
        case CMD_MESSAGE_HISTORY:
            return std::make_unique<im::message::PullMessageRequest>();
            
        case CMD_DELETE_MESSAGE:
        case CMD_RECALL_MESSAGE:
            return std::make_unique<im::base::BaseResponse>();  // 使用基础响应作为占位
            
        case CMD_MESSAGE_ACK:
        case CMD_MESSAGE_DELIVERED:
            return std::make_unique<im::base::BaseResponse>();  // 使用基础响应作为占位
            
        // 好友相关
        case CMD_ADD_FRIEND:
        case CMD_REMOVE_FRIEND:
        case CMD_HANDLE_FRIEND_REQUEST:
            return std::make_unique<im::friend_::FriendRequest>();
            
        case CMD_GET_FRIEND_LIST:
        case CMD_GET_FRIEND_REQUESTS:
            return std::make_unique<im::friend_::GetFriendListRequest>();
            
        case CMD_SEARCH_USER:
            return std::make_unique<im::user::SearchUserRequest>();
            
        // 群组相关
        case CMD_CREATE_GROUP:
            return std::make_unique<im::group::CreateGroupRequest>();
            
        case CMD_GET_GROUP_INFO:
        case CMD_GET_GROUP_LIST:
            return std::make_unique<im::group::GetGroupInfoRequest>();
            
        case CMD_MODIFY_GROUP_INFO:
            return std::make_unique<im::group::ModifyGroupInfoRequest>();
            
        case CMD_INVITE_MEMBER:
        case CMD_KICK_MEMBER:
            return std::make_unique<im::group::GroupMemberRequest>();
            
        case CMD_APPLY_JOIN_GROUP:
        case CMD_QUIT_GROUP:
            return std::make_unique<im::group::JoinGroupRequest>();
            
        case CMD_GET_GROUP_MEMBERS:
            return std::make_unique<im::group::GetGroupMembersRequest>();
            
        case CMD_GET_GROUP_MESSAGES:
            return std::make_unique<im::group::GetGroupMessagesRequest>();
            
        case CMD_TRANSFER_GROUP_OWNER:
        case CMD_SET_GROUP_ADMIN:
            return std::make_unique<im::group::GroupAdminRequest>();
            
        // 推送相关
        case CMD_PUSH_MESSAGE:
        case CMD_PUSH_BATCH_MESSAGE:
        case CMD_PUSH_NOTIFICATION:
        case CMD_PUSH_SYSTEM:
        case CMD_PUSH_OFFLINE:
            return std::make_unique<im::push::PushRequest>();
            
        // 其他
        case CMD_HEARTBEAT:
            return std::make_unique<im::base::BaseResponse>();  // 心跳使用基础响应
            
        case CMD_SERVER_NOTIFY:
        case CMD_CLIENT_ERROR:
            return std::make_unique<im::base::BaseResponse>();
            
        default:
            return nullptr;  // 不支持的命令
    }
}

// ================================
// MessageFactory 实现
// ================================

std::unique_ptr<InternalMessage> MessageFactory::create_from_http(
    const HttpMessageParser::HttpParseResult& parse_result,
    const std::string& session_id,
    const std::string& client_ip) {
    
    if (!parse_result.is_valid) {
        return nullptr;
    }
    
    auto internal_msg = std::make_unique<InternalMessage>();
    
    // 设置消息头信息
    im::base::IMHeader header;
    header.set_version("1.0");
    header.set_seq(0);  // HTTP请求不使用序列号
    header.set_cmd_id(parse_result.cmd_id);
    header.set_token(parse_result.token);
    header.set_device_id(parse_result.device_id);
    header.set_platform(parse_result.platform);
    
    // 设置时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    header.set_timestamp(static_cast<uint64_t>(timestamp));
    
    // 创建对应的Protobuf消息对象
    auto protobuf_msg = create_protobuf_message(parse_result.cmd_id);
    if (protobuf_msg && !parse_result.body_json.empty()) {
        // 将JSON转换为Protobuf消息
        if (!json_to_protobuf(parse_result.body_json, protobuf_msg.get())) {
            return nullptr;  // JSON转换失败
        }
    }
    
    // 设置消息上下文
    InternalMessage::MessageContext context;
    context.protocol = InternalMessage::Protocol::HTTP;
    context.session_id = session_id;
    context.client_ip = client_ip;
    context.timestamp = now;
    
    // 设置InternalMessage的内部数据
    internal_msg->set_header(std::move(header));
    internal_msg->set_context(std::move(context));
    
    if (protobuf_msg) {
        internal_msg->set_protobuf_message(std::move(protobuf_msg));
    } else {
        internal_msg->set_json_body(parse_result.body_json);
    }
    
    return internal_msg;
}

std::unique_ptr<InternalMessage> MessageFactory::create_from_websocket(
    const WebSocketMessageParser::WSParseResult& parse_result,
    const std::string& session_id,
    const std::string& client_ip) {
    
    if (!parse_result.is_valid) {
        return nullptr;
    }
    
    auto internal_msg = std::make_unique<InternalMessage>();
    
    // WebSocket消息已经有完整的header
    // 设置消息上下文
    InternalMessage::MessageContext context;
    context.protocol = InternalMessage::Protocol::WEBSOCKET;
    context.session_id = session_id;
    context.client_ip = client_ip;
    context.timestamp = std::chrono::system_clock::now();
    
    // 设置InternalMessage的内部数据
    internal_msg->set_header(parse_result.header);
    internal_msg->set_context(std::move(context));
    
    // 复制protobuf消息（需要克隆）
    if (parse_result.message) {
        // 创建一个新的消息对象并复制数据
        auto new_message = create_protobuf_message(parse_result.header.cmd_id());
        if (new_message) {
            new_message->CopyFrom(*parse_result.message);
            internal_msg->set_protobuf_message(std::move(new_message));
        }
    }
    
    return internal_msg;
}

std::unique_ptr<google::protobuf::Message> MessageFactory::create_protobuf_message(uint32_t cmd_id) {
    // 复用WebSocketMessageParser的实现
    WebSocketMessageParser temp_parser;
    return temp_parser.create_message_by_cmd(cmd_id);
}

bool MessageFactory::json_to_protobuf(const std::string& json_str, 
                                     google::protobuf::Message* message) {
    if (!message || json_str.empty()) {
        return false;
    }
    
    try {
        // 使用Protobuf的JSON工具进行转换
        google::protobuf::util::JsonParseOptions options;
        options.ignore_unknown_fields = true;  // 忽略未知字段
        
        auto status = google::protobuf::util::JsonStringToMessage(json_str, message, options);
        return status.ok();
        
    } catch (const std::exception& e) {
        // 记录错误日志
        return false;
    }
}

// ================================
// MessageParser 主类实现
// ================================

std::unique_ptr<InternalMessage> MessageParser::parse_http_request(
    const httplib::Request& req, 
    const std::string& session_id) {
    
    if (!http_parser_) {
        http_parser_ = std::make_unique<HttpMessageParser>();
    }
    
    if (!message_factory_) {
        message_factory_ = std::make_unique<MessageFactory>();
    }
    
    // 解析HTTP请求
    auto parse_result = http_parser_->parse(req);
    if (!parse_result.is_valid) {
        return nullptr;
    }
    
    // 获取客户端IP（从请求中提取）
    std::string client_ip = "unknown";
    auto x_forwarded_for = req.headers.find("X-Forwarded-For");
    if (x_forwarded_for != req.headers.end()) {
        client_ip = x_forwarded_for->second;
    } else {
        auto x_real_ip = req.headers.find("X-Real-IP");
        if (x_real_ip != req.headers.end()) {
            client_ip = x_real_ip->second;
        }
    }
    
    // 使用MessageFactory创建InternalMessage
    return message_factory_->create_from_http(parse_result, session_id, client_ip);
}

std::unique_ptr<InternalMessage> MessageParser::parse_websocket_message(
    const std::string& raw_message,
    const std::string& session_id) {
    
    if (!websocket_parser_) {
        websocket_parser_ = std::make_unique<WebSocketMessageParser>();
    }
    
    if (!message_factory_) {
        message_factory_ = std::make_unique<MessageFactory>();
    }
    
    // 解析WebSocket消息
    auto parse_result = websocket_parser_->parse(raw_message);
    if (!parse_result.is_valid) {
        return nullptr;
    }
    
    // 对于WebSocket连接，客户端IP通常需要从连接上下文获取
    // 这里先使用占位符，实际使用时需要从WebSocket会话中获取
    std::string client_ip = "websocket_client";
    
    // 使用MessageFactory创建InternalMessage
    return message_factory_->create_from_websocket(parse_result, session_id, client_ip);
}

}  // namespace gateway
}  // namespace im