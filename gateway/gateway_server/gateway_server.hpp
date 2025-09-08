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

#include "../auth/multi_platform_auth.hpp"
#include "../connection_manager/connection_manager.hpp"
#include "../message_processor/coro_message_processor.hpp"
#include "../message_processor/message_parser.hpp"
#include "../message_processor/message_processor.hpp"
#include "../router/router_mgr.hpp"
#include "boost/asio/ssl/context.hpp"

#include <httplib.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>


namespace im {
namespace gateway {

using im::network::IOServicePool;
using im::network::SessionPtr;
using im::network::WebSocketServer;
using im::network::WebSocketSession;
using im::utils::ConfigManager;
using im::utils::LogManager;
using message_handler = std::function<Task<CoroProcessorResult>(const UnifiedMessage& msg)>;


class GatewayServer : public std::enable_shared_from_this<GatewayServer> {
public:
    // GatewayServer(const std::string &config_path); // 后续完善大一统配置文件逻辑
    GatewayServer(const GatewayServer&) = delete;
    GatewayServer(const std::string platform_strategy_config, const std::string router_mgr,
                  uint16_t ws_port = 8080, uint16_t http_port = 8081);
    ~GatewayServer();

    // 启动网关服务
    void start();

    void stop();

    bool init_server(uint16_t ws_port, uint16_t http_port,
                     const std::string& log_path = "");  // 初始化服务器,内调用相关组件的初始化函数


    std::string get_server_stats() const;

    bool is_running() const { return is_running_; }

    // ConnectionManager集成接口
    bool push_message_to_user(const std::string& user_id, const std::string& message);
    bool push_message_to_device(const std::string& user_id, const std::string& device_id,
                                const std::string& platform, const std::string& message);
    size_t get_online_count() const;

    bool register_message_handlers(uint32_t cmd_id, message_handler handler);

private:
    // bool init_network_components();
    void init_ws_server(uint16_t port);  // param : ioc , ssl_context ,port , message_callback
    void init_http_server(uint16_t port);

    // bool init_core_components();
    void init_conn_mgr();       // param: platform_strategy_configfile , ws_server
    void init_msg_parser();     // param: routerMgr/router_configfile
    void init_msg_processor();  // param: routerMgr/router_configfile , auth_mgr/auth_configfile ,
                                // CoroProcessingOptions

    // bool init_utils_components();
    void init_logger(const std::string& log_folder = "");
    void init_io_service_pool();  // 初始化IOServicePool

    void register_message_handlers();

    // WebSocket连接事件处理
    void on_websocket_connect(SessionPtr session);
    void on_websocket_disconnect(SessionPtr session);

    // Token验证和连接管理
    bool verify_and_bind_connection(SessionPtr session, const std::string& token);

    // 安全相关
    void schedule_unauthenticated_timeout(SessionPtr session);
    bool is_session_authenticated(SessionPtr session) const;


    // 网络服务组件
    std::shared_ptr<IOServicePool> io_service_pool_;
    std::unique_ptr<WebSocketServer> websocket_server_;
    boost::asio::ssl::context ssl_ctx_;  // ssl_context必须要初始化
    std::unique_ptr<httplib::Server> http_server_;

    // 网关核心组件
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<RouterManager> router_mgr_;
    std::unique_ptr<MessageParser> msg_parser_;
    std::unique_ptr<CoroMessageProcessor> msg_processor_;

    // 配置和日志管理器
    std::shared_ptr<ConfigManager> config_mgr_;  // 后续用于读取统一配置
    std::shared_ptr<spdlog::logger> server_logger;

    std::atomic<bool> is_running_;
    std::string psc_path_;  // platform_strategy_config_path_
};



}  // namespace gateway
}  // namespace im

#endif  // GATEWAY_SERVER_HPP