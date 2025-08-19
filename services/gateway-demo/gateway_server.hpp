#ifndef GATEWAY_SERVER_HPP
#define GATEWAY_SERVER_HPP

/******************************************************************************
 *
 * @file       gateway_server.hpp
 * @brief      网关服务器类，负责处理客户端WebSocket连接和消息路由
 *
 * @author     MyChat Development Team
 * @date       2025/08/06
 *
 *****************************************************************************/

#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "common/network/websocket_server.hpp"
#include "common/network/protobuf_codec.hpp"
#include "common/utils/log_manager.hpp"

namespace im {
namespace gateway {

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// 前置声明
class MessageRouter;
class ConnectionManager;

class GatewayServer {
public:
    /**
     * @brief 构造函数
     * @param ioc IO上下文
     * @param ssl_ctx SSL上下文
     * @param port 监听端口
     */
    GatewayServer(net::io_context& ioc, ssl::context& ssl_ctx, unsigned short port);
    
    /**
     * @brief 启动网关服务器
     */
    void start();
    
    /**
     * @brief 停止网关服务器
     */
    void stop();
    
private:
    /**
     * @brief 处理WebSocket消息
     * @param session WebSocket会话
     * @param buffer 消息缓冲区
     */
    void handle_message(network::SessionPtr session, boost::beast::flat_buffer&& buffer);
    
    /**
     * @brief 处理会话关闭事件
     * @param session WebSocket会话
     */
    void on_session_closed(network::SessionPtr session);
    
private:
    std::unique_ptr<network::WebSocketServer> websocket_server_;
    std::unique_ptr<MessageRouter> message_router_;
    std::unique_ptr<ConnectionManager> connection_manager_;
};

} // namespace gateway
} // namespace im

#endif // GATEWAY_SERVER_HPP