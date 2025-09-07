#include <functional>
#include <memory>

#include "../../common/utils/coroutine_manager.hpp"
#include "../../common/utils/http_utils.hpp"
#include "gateway_server.hpp"
#include "httplib.h"

namespace im {
namespace gateway {

using im::common::CoroutineManager;
using im::common::Task;


// GatewayServer::GatewayServer(const std::string& config_file)
//         : ssl_ctx_(boost::asio::ssl::context::tlsv12_server), is_running_(false) {}

GatewayServer::GatewayServer(const std::string platform_strategy_config,
                             const std::string router_mgr,
                             const std::string,
                             uint16_t ws_port)
        : auth_mgr_(std::make_shared<MultiPlatformAuthManager>(platform_strategy_config))
        , router_mgr_(std::make_shared<RouterManager>(router_mgr))
        , ssl_ctx_(boost::asio::ssl::context::tlsv12_server)
        , is_running_(false)
        , psc_path_(platform_strategy_config) {
    init_server(ws_port);
}

bool GatewayServer::init_server(uint16_t ws_port, const std::string& log_path) {
    try {
        // 初始化日志系统
        init_logger(log_path);

        // 初始化网络服务器
        init_ws_server(ws_port);
        init_http_server();

        // 初始化连接管理器和消息解析器处理器
        // 一定要注意初始化顺序,连接管理器依赖 ws_server来进行session管理
        init_conn_mgr();
        init_msg_parser();
        init_msg_processor();

        return true;
    } catch (...) {
        return false;
    }
}


void GatewayServer::init_logger(const std::string& log_path){
    this->log_mgr_= std::make_unique<utils::LogManager>();
    std::string path = log_path;
    if(!log_path.ends_with("/")){
        path += "/";
    }

    log_mgr_->SetLogToFile("gateway_server", path + "gateway_server.log");
    server_logger = LogManager::GetLogger("gateway_server");
    

    log_mgr_->SetLogToFile("io_service_pool",path + "io_service_pool.log");
    log_mgr_->SetLogToFile("websocket_server",path + "websocket_server.log");
    log_mgr_->SetLogToFile("websocket_session",path + "websocket_session.log");
    log_mgr_->SetLogToFile("connection_manager",path + "connection_manager.log");

    log_mgr_->SetLogToFile("coro_message_processor",path + "coro_message_processor.log");
    log_mgr_->SetLogToFile("message_parser",path + "message_parser.log");
    log_mgr_->SetLogToFile("message_processor",path + "message_processor.log");

    log_mgr_->SetLogToFile("router_manager",path + "router_manager.log");
    log_mgr_->SetLogToFile("service_router",path + "router_manager.log");
    log_mgr_->SetLogToFile("http_router",path + "router_manager.log");
}

void GatewayServer::init_ws_server(uint16_t port) {
    try {
        // 构造websocket服务器消息处理函数
        std::function<void(SessionPtr, beast::flat_buffer&&)>
        message_handler([this, self = shared_from_this()](SessionPtr sessionPtr,
                                                          beast::flat_buffer&& buffer) -> void {
            auto result = this->msg_parser_->parse_websocket_message_enhanced(
                    beast::buffers_to_string(buffer.data()), sessionPtr->get_session_id());
            if (!result.success) {
                server_logger->error(
                        "parse message error in gateway.ws_server callback; error_message:",
                        result.error_message,
                        " error_code:",
                        result.error_code);
            }
            if (msg_processor_) {
                auto coro_task =
                        this->msg_processor_->coro_process_message(std::move(result.message));

                auto&& coro_mgr = CoroutineManager::getInstance();

                coro_mgr.schedule([this,
                                   task = std::move(coro_task),
                                   sessionPtr]() mutable -> im::common::Task<void> {
                    try {
                        auto result = co_await task;
                        // 处理结果
                        if (result.status_code != 0) {
                            // 处理错误情况
                            this->server_logger->error("处理消息时发生错误: ",
                                                       result.error_message);
                        } else {
                            // 发送响应给客户端
                            if (!result.protobuf_message.empty()) {
                                sessionPtr->send(result.protobuf_message);
                            } else if (!result.json_body.empty()) {
                                sessionPtr->send(result.json_body);
                            }

                            // 不用做空处理，在消息处理逻辑中会做错误返回的空处理
                            // else{
                            // }
                        }
                    } catch (const std::exception& e) {
                        // 处理协程执行异常
                        this->server_logger->error(
                                "CoroMessageProcessor exception in gateway.ws_server callback: {}",
                                e.what());
                    }
                }());
            } else {
                server_logger->error("CoroMessageProcessor is not initialized.");
                init_msg_processor();
            }
        });


        websocket_server_ = std::make_unique<WebSocketServer>(
                io_service_pool_->GetIOService(), ssl_ctx_, port, message_handler);
        websocket_server_->start();

    } catch (...) {
        server_logger->error("Failed to start websocket server.");
        throw std::runtime_error("Failed to start websocket server.");
    }
}
void GatewayServer::init_http_server() {
    this->http_server_ = std::make_unique<httplib::Server>();

    try {
        std::function<void(const httplib::Request& req, httplib::Response& res)> http_callback(
                [this](const httplib::Request& req, httplib::Response& res) {
                    try {
                        auto parse_result = msg_parser_->parse_http_request_enhanced(req);
                        if (!parse_result.success) {
                            // 解析错误
                            server_logger->error(
                                    "parse message error in gateway.http_server callback; "
                                    "error_message:",
                                    parse_result.error_message,
                                    " error_code:",
                                    parse_result.error_code);
                            // res.status = 500;
                            HttpUtils::buildResponse(res, 500, "", parse_result.error_message);
                            return;
                        }
                        if (msg_processor_) {
                            auto coro_task = this->msg_processor_->coro_process_message(
                                    std::move(parse_result.message));

                            auto&& coro_mgr = CoroutineManager::getInstance();

                            // 注意捕获req 和 res的引用
                            coro_mgr.schedule([&parse_result,
                                               &req,
                                               &res,
                                               this,
                                               task = std::move(coro_task)]() mutable
                                              -> im::common::Task<void> {
                                try {
                                    auto result = co_await task;
                                    // 处理结果
                                    if (result.status_code != 0) {
                                        // 处理错误情况
                                        this->server_logger->error("处理消息时发生错误: ",
                                                                   result.error_message);
                                        // res.status=result.status_code;
                                        // res.status = 500;  // 消息解析处理错误应为服务器内部错误
                                        HttpUtils::buildResponse(res, result.status_code,
                                                                 result.json_body,
                                                                 result.error_message);
                                        co_return;
                                    } else {
                                        if (!result.json_body.empty()) {
                                            std::string reqInfo(
                                                    parse_result.message->format_info().str());

                                            // 获取并解析状态码确定日志等级
                                            const int status_code =
                                                    HttpUtils::statusCodeFromJson(result.json_body);

                                            switch (HttpUtils::parseStatusCode(status_code)) {
                                                case HttpUtils::StatusLevel::INFO:
                                                    // server_logger->info("http
                                                    // request:","statius_code:",status_code);
                                                    break;
                                                case HttpUtils::StatusLevel::WARNING:
                                                    server_logger->warn("warning in http request:",
                                                                        "statius_code:",
                                                                        status_code);
                                                    break;
                                                case HttpUtils::StatusLevel::ERROR:
                                                    server_logger->error("error in http request:",
                                                                         "statius_code:",
                                                                         status_code,
                                                                         "requestInfo:",
                                                                         reqInfo);
                                                    // 此处应该不用抛出异常，而是将原本的错误码照常返回
                                                    // throw std::runtime_error(
                                                    //         HttpUtils::err_msgFromJson(
                                                    //                 result.json_body));
                                            }
                                            // 构建正常返回体
                                            HttpUtils::buildResponse(res, result.json_body);
                                        }
                                    }
                                } catch (const std::exception& e) {
                                    // 处理协程执行异常
                                    this->server_logger->error(
                                            "CoroMessageProcessor exception in gateway.ws_server "
                                            "callback: {}",
                                            e.what());
                                    HttpUtils::buildResponse(res, 500, "", e.what());
                                }
                            }());
                        }

                    } catch (...) {
                        this->server_logger->error(
                                "[GatewayServer::start] Exception caught in http_callback");
                        HttpUtils::buildResponse(res, 500, "", "Exception caught in http_callback");
                    }
                });

        http_server_->Get(".*", http_callback);
        http_server_->Post(".*", http_callback);
    } catch (...) {
        this->server_logger->error("[GatewayServer::http_server] Exception caught in init http_server");
        throw std::runtime_error("Failed to start http server");
    }
}

void GatewayServer::init_conn_mgr(){
    conn_mgr_ = std::make_unique<ConnectionManager>(psc_path_,websocket_server_.get());
}

void GatewayServer::init_msg_parser(){
    msg_parser_ = std::make_unique<MessageParser>(router_mgr_);
}

void GatewayServer::init_msg_processor(){
    msg_processor_ = std::make_unique<CoroMessageProcessor>(router_mgr_,auth_mgr_);
}


}  // namespace gateway
}  // namespace im
