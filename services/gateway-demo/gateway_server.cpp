#include "gateway_server.hpp"
#include "message_router.hpp"
#include "connection_manager.hpp"

namespace im {
namespace gateway {

GatewayServer::GatewayServer(net::io_context& ioc, ssl::context& ssl_ctx, unsigned short port) {
    // 初始化各个组件
    websocket_server_ = std::make_unique<network::WebSocketServer>(ioc, ssl_ctx, port,
        [this](network::SessionPtr session, boost::beast::flat_buffer&& buffer) {
            this->handle_message(session, std::move(buffer));
        });
        
    message_router_ = std::make_unique<MessageRouter>();
    
    // 初始化ConnectionManager，传递WebSocketServer指针
    connection_manager_ = std::make_unique<ConnectionManager>("../../config/platform_config.json", websocket_server_.get());
}

void GatewayServer::start() {
    if (websocket_server_) {
        websocket_server_->start();
    }
}

void GatewayServer::stop() {
    if (websocket_server_) {
        websocket_server_->stop();
    }
}

void GatewayServer::handle_message(network::SessionPtr session, boost::beast::flat_buffer&& buffer) {
    // 处理WebSocket消息
    // 这里应该调用MessageRouter来处理消息
    if (message_router_) {
        // 将buffer转换为字符串并传递给消息路由器
        std::string message(static_cast<const char*>(buffer.data().data()), buffer.size());
        // message_router_->route_message(session, message);
    }
}

void GatewayServer::on_session_closed(network::SessionPtr session) {
    // 处理会话关闭事件
    // 通知ConnectionManager移除会话
    if (connection_manager_ && session) {
        connection_manager_->remove_connection(session);
    }
}

} // namespace gateway
} // namespace im