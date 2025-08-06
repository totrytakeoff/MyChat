#ifndef GATEWAY_SERVER_HPP
#define GATEWAY_SERVER_HPP

/******************************************************************************
 *
 * @file       gateway_server.hpp
 * @brief      gateway server 网关主服务类
 *
 * @author     myself
 * @date       2025/08/06
 *
 *****************************************************************************/


#include "../../common/network/IOService_pool.hpp"
#include "../../common/network/tcp_server.hpp"
#include "../../common/network/websocket_server.hpp"
#include "../../common/utils/config_mgr.hpp"
#include "../../common/utils/log_manager.hpp"

#include <httplib.h>

#include <memory>
#include <string>

namespace im {
namespace gateway {

class ConnectionManager;
class AuthManager;
class RouteManager;
class MessageProcessor;

using im::network::TCPServer;
using im::network::TCPSession;
using im::network::WebSocketServer;
using im::network::WebSocketSession;
using im::network::IOServicePool;
using im::utils::ConfigManager;
using im::utils::LogManager;

class GatewayServer {
public:
    GatewayServer();
    ~GatewayServer();

    // 启动网关服务
    void start();

    void stop();

    // 消息处理入口
    void handle_websocket_message(std::shared_ptr<WebSocketSession> session,
                                  const std::string& message);

    void handle_tcp_message(std::shared_ptr<TCPSession> session, const std::string& message);

    void handle_http_request(const httplib::Request& req, httplib::Response& res);

private:
    bool InitNetwork();
    bool InitManagers();

    // 网络服务组件
    std::shared_ptr<IOServicePool> io_service_pool_;
    std::unique_ptr<TCPServer> tcp_server_;
    std::unique_ptr<WebSocketServer> websocket_server_;

    // 网关核心组件
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::unique_ptr<AuthManager> auth_mgr_;
    std::unique_ptr<RouteManager> route_mgr_;

    //配置和日志管理器
    std::shared_ptr<ConfigManager> config_mgr_;
    std::shared_ptr<LogManager> log_mgr_;

    bool is_running_ ;
};



}  // namespace gateway
}  // namespace im

#endif  // GATEWAY_SERVER_HPP