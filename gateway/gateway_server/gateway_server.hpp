#ifndef GATEWAY_SERVER_HPP
#define GATEWAY_SERVER_HPP

/******************************************************************************
 *
 * @file       gateway_server.hpp
 * @brief      gateway server 网关主服务类
 *
 * @author     myself
 * @date       2025/09/06
 *
 *****************************************************************************/


#include "../../common/network/IOService_pool.hpp"
#include "../../common/network/tcp_server.hpp"
#include "../../common/network/websocket_server.hpp"

#include "../../common/utils/config_mgr.hpp"
#include "../../common/utils/log_manager.hpp"

#include "../connection_manager/connection_manager.hpp"
#include "../auth/multi_platform_auth.hpp"
#include "../router/router_mgr.hpp"
#include "../message_processor/message_processor.hpp"
#include "../message_processor/coro_message_processor.hpp"
#include "../message_processor/message_parser.hpp"
#include "boost/asio/ssl/context.hpp"

#include <httplib.h>

#include <memory>
#include <string>
#include <atomic>


namespace im {
namespace gateway {

using im::utils::LogManager;
using im::utils::ConfigManager;
using im::network::IOServicePool;
using im::network::WebSocketServer;
using im::network::WebSocketSession;
using im::network::ProtobufCodec;



class GatewayServer :public std::enable_shared_from_this<GatewayServer>{
public:
    GatewayServer(const std::string &config_path);
    GatewayServer(const GatewayServer &) = delete;
    GatewayServer(const std::string platform_strategy_config,const std::string router_mgr,const std::string , uint16_t ws_port);
    ~GatewayServer();

    // 启动网关服务
    void start();

    void stop();

    bool init_server();// 初始化服务器,内调用相关组件的初始化函数


    std::string get_server_stats() const;

    bool is_running() const { return is_running_; }


private:
    // bool init_network_components();
    bool init_ws_server(uint16_t port); // param : ioc , ssl_context ,port , message_callback
    bool init_http_server(); 

    // bool init_core_components();
    bool init_conn_mgr(const std::string &config_path = ""); // param: platform_strategy_configfile , ws_server 
    bool init_auth_mgr(const std::string &config_path = ""); // param : auth_configfile(platform_strategy_configfile)
    bool init_router_mgr(const std::string &config_path = "");// param: router_configfile
    bool init_msg_parser();  //param: routerMgr/router_configfile
    bool init_msg_processor(); // param: routerMgr/router_configfile , auth_mgr/auth_configfile , CoroProcessingOptions

    // bool init_utils_components();
    bool init_logger(const std::string& log_folder = ""); 

    bool register_message_handlers();


    // 网络服务组件
    std::shared_ptr<IOServicePool> io_service_pool_;
    std::unique_ptr<WebSocketServer> websocket_server_;
    boost::asio::ssl::context ssl_ctx_;
    std::unique_ptr<httplib::Server> http_server_;

    // 网关核心组件
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::unique_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::unique_ptr<RouterManager> router_mgr_;
    std::unique_ptr<MessageParser> msg_parser_;
    std::unique_ptr<CoroMessageProcessor> msg_processor_;

    //配置和日志管理器
    std::shared_ptr<ConfigManager> config_mgr_;
    std::shared_ptr<LogManager> log_mgr_;
    std::shared_ptr<spdlog::logger> server_logger;

    std::atomic<bool> is_running_ ;
};



}  // namespace gateway
}  // namespace im

#endif  // GATEWAY_SERVER_HPP