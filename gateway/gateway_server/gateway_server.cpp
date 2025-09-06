#include <functional>
#include <memory>
#include "../../common/utils/coroutine_manager.hpp"
#include "gateway_server.hpp"

namespace im {
namespace gateway {

using im::common::CoroutineManager;
using im::common::Task;


GatewayServer::GatewayServer(const std::string& config_file)
        : ssl_ctx_(boost::asio::ssl::context::tlsv12_server) {}

GatewayServer::GatewayServer(const std::string platform_strategy_config,
                             const std::string router_mgr,
                             const std::string,
                             uint16_t ws_port)
        : auth_mgr_(std::make_unique<MultiPlatformAuthManager>(platform_strategy_config))
        , router_mgr_(std::make_unique<RouterManager>(router_mgr))
        , ssl_ctx_(boost::asio::ssl::context::tlsv12_server) {
    init_server();
}

bool GatewayServer::init_server() {
    if (!init_logger("") || !init_http_server() || !init_ws_server(8000) || !init_auth_mgr() ||
        !init_conn_mgr("") || !init_router_mgr() || !init_msg_parser() || !init_msg_processor()) {
        return false;
    }
    return true;
}


bool GatewayServer::init_ws_server(uint16_t port) {
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
                        }
                    } catch (const std::exception& e) {
                        // 处理协程执行异常
                        this->server_logger->error(
                                "CoroMessageProcessor exception in gateway.ws_server callback: {}",
                                e.what());
                    }
                }());
            }
        });


        websocket_server_ = std::make_unique<WebSocketServer>(
                io_service_pool_->GetIOService(), ssl_ctx_, port, message_handler);
        websocket_server_->start();
        return true;
    } catch (...) {
        return false;
    }
}
bool GatewayServer::init_http_server() { return true; }



}  // namespace gateway
}  // namespace im
