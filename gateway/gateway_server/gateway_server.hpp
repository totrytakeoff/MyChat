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
#include "../command_handlers/gateway_command_handler_registry.hpp"
#include "../message_processor/coro_message_processor.hpp"
#include "../message_processor/message_parser.hpp"
#include "../message_processor/message_processor.hpp"
#include "../router/router_mgr.hpp"
#include "boost/asio/ssl/context.hpp"

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <thread>

namespace odb { namespace pgsql { class database; } }
namespace grpc { class Server; }

namespace im::gateway { class UserServiceAdapter; }
namespace im::gateway { class MessageServiceAdapter; }
namespace im::gateway { class FriendServiceAdapter; }
namespace im::gateway { class GroupServiceAdapter; }

namespace im::service::user { class UserService; }
namespace im::service::message { class MessageService; }
namespace im::service::friend_ { class FriendService; }

#ifdef IM_ENABLE_MESSAGE_WS
namespace im::gateway { class MessageWsHandler; }
#endif

#ifdef IM_ENABLE_PUSH_SERVICE
namespace im::gateway { class PushService; }
#endif

#if defined(IM_ENABLE_MESSAGE_WS) || defined(IM_ENABLE_GROUP_MESSAGE_HTTP)
namespace im::service::push { class PushNotifier; }
#endif

#ifdef IM_ENABLE_REMOTE_PUSH_NOTIFIER
namespace im::gateway { class RemotePushNotifier; }
namespace im::gateway { class GatewayPushDeliveryService; }
#endif

#if defined(IM_ENABLE_GROUP_HTTP) || defined(IM_ENABLE_GROUP_MESSAGE_HTTP)
namespace im::service::group { class GroupService; }
namespace im::service::group { class GroupMessageService; }
#endif

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

    // 注释掉协程版本的注册方法
    // bool register_message_handlers(uint32_t cmd_id, message_handler handler) { return msg_processor_->register_coro_processor(cmd_id, handler) == 0; }
    bool register_message_handlers(uint32_t cmd_id, std::function<ProcessorResult(const UnifiedMessage&)> handler);
    bool force_register_handler(uint32_t cmd_id, std::function<ProcessorResult(const UnifiedMessage&)> handler);  // Test helper
    static ProcessorResult handle_heartbeat_message(
            const UnifiedMessage& msg,
            const std::shared_ptr<MultiPlatformAuthManager>& auth_mgr);

    // 测试辅助：注册协程处理器（用于WebSocket路径）
    int register_coro_message_handler(uint32_t cmd_id, CoroProcessorFunction handler) {
        return coro_msg_processor_ ?coro_msg_processor_->register_coro_processor(cmd_id, std::move(handler)) : -2;
    }

private:
    // bool init_network_components();
    void init_ws_server(uint16_t port);  // param : ioc , ssl_context ,port , message_callback
    void init_http_server(uint16_t port);

    // bool init_core_components();
    void init_conn_mgr();       // param: platform_strategy_configfile , ws_server
    void init_msg_parser();     // param: routerMgr/router_configfile
    void init_msg_processor();  // param: routerMgr/router_configfile , auth_mgr/auth_configfile ,
                                // CoroProcessingOptions
    void init_database_runtime();
    void init_user_runtime();
    void init_message_runtime();
    void init_friend_runtime();
    void init_group_runtime();
    void init_group_message_runtime();
    void init_message_ws_runtime();

    // bool init_utils_components();
    void init_logger(const std::string& log_folder = "");
    void init_io_service_pool();  // 初始化IOServicePool

    void register_message_handlers();
    void refresh_runtime_registry();

#if defined(IM_ENABLE_USER_HTTP) || defined(IM_ENABLE_FRIEND_HTTP) || defined(IM_ENABLE_GROUP_HTTP) || defined(IM_ENABLE_GROUP_MESSAGE_HTTP)
    void ensure_user_service();
#endif
#if defined(IM_ENABLE_GROUP_HTTP) || defined(IM_ENABLE_GROUP_MESSAGE_HTTP)
    void ensure_group_services();
#endif

#ifdef IM_ENABLE_REMOTE_PUSH_NOTIFIER
    void start_gateway_push_delivery_server(const std::string& listen_address);
    void stop_gateway_push_delivery_server();
#endif

    // WebSocket连接事件处理
    void on_websocket_connect(SessionPtr session);
    void on_websocket_disconnect(SessionPtr session);

    // Token验证和连接管理
    bool verify_and_bind_connection(SessionPtr session, const std::string& token);

    // 安全相关
    void schedule_unauthenticated_timeout(SessionPtr session);
    void schedule_delayed_close(SessionPtr session, std::chrono::milliseconds delay);
    bool is_session_authenticated(SessionPtr session) const;

    struct HttpRouteStats {
        uint64_t count = 0;
        uint64_t total_ms = 0;
        uint64_t max_ms = 0;
    };

    struct HttpStats {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> inflight_requests{0};
        std::atomic<uint64_t> status_2xx{0};
        std::atomic<uint64_t> status_4xx{0};
        std::atomic<uint64_t> status_5xx{0};
        std::atomic<uint64_t> status_other{0};
        mutable std::mutex routes_mutex;
        std::unordered_map<std::string, HttpRouteStats> routes;
    };

    struct ServiceRuntimeState {
        std::string name;
        std::string mode = "local";
        std::string remote_endpoint;
        int timeout_ms = 200;
        bool local_bound = false;
        bool remote_bound = false;
    };

    void record_http_response(const httplib::Request& req,
                              const httplib::Response& res,
                              uint64_t duration_ms);
    std::string format_http_route_stats() const;
    ServiceRuntimeState& load_service_runtime_state(const ConfigManager& config,
                                                    const std::string& name,
                                                    int default_timeout_ms = 200);
    void mark_service_runtime_local(const std::string& name);
    void mark_service_runtime_remote(const std::string& name,
                                     const std::string& endpoint);
    void warn_unknown_runtime_mode(const ServiceRuntimeState& state,
                                   const std::string& fallback) const;
    static bool is_remote_mode(const ServiceRuntimeState& state);
    static bool is_local_mode(const ServiceRuntimeState& state);


    // 网络服务组件
    std::shared_ptr<IOServicePool> io_service_pool_;
    std::unique_ptr<WebSocketServer> websocket_server_;
    boost::asio::ssl::context ssl_ctx_;  // ssl_context必须要初始化
    std::unique_ptr<httplib::Server> http_server_;
    std::thread http_thread_;

    // 网关核心组件
    std::unique_ptr<ConnectionManager> conn_mgr_;
    std::shared_ptr<MultiPlatformAuthManager> auth_mgr_;
    std::shared_ptr<RouterManager> router_mgr_;
    std::unique_ptr<MessageParser> msg_parser_;
    std::unique_ptr<CoroMessageProcessor> coro_msg_processor_;
    std::unique_ptr<MessageProcessor> msg_processor_;

    // 配置和日志管理器
    std::shared_ptr<ConfigManager> config_mgr_;  // 后续用于读取统一配置
    std::shared_ptr<spdlog::logger> server_logger;

    std::atomic<bool> is_running_;
    std::atomic<size_t> ws_inflight_messages_{0};
    size_t max_ws_inflight_messages_{4096};
    HttpStats http_stats_;
    std::unordered_map<std::string, ServiceRuntimeState> service_runtime_;
    GatewayRuntimeRegistry runtime_registry_;
    std::string psc_path_;     // platform_strategy_config_path_
    std::string config_path_;  // gateway/router/auth shared config path for the MVP

#if defined(IM_ENABLE_USER_HTTP) || defined(IM_ENABLE_MESSAGE_HTTP) || defined(IM_ENABLE_FRIEND_HTTP) || defined(IM_ENABLE_GROUP_HTTP) || defined(IM_ENABLE_GROUP_MESSAGE_HTTP)
    // ODB database instance for service integration
    std::shared_ptr<odb::pgsql::database> odb_db_;
#endif

#ifdef IM_ENABLE_MESSAGE_WS
    std::unique_ptr<MessageWsHandler> message_ws_handler_;
#endif

#if defined(IM_ENABLE_MESSAGE_WS) || defined(IM_ENABLE_GROUP_MESSAGE_HTTP)
    im::service::push::PushNotifier* push_notifier_ = nullptr;
#endif

#ifdef IM_ENABLE_PUSH_SERVICE
    std::unique_ptr<PushService> push_service_;
#endif

#ifdef IM_ENABLE_REMOTE_PUSH_NOTIFIER
    std::unique_ptr<RemotePushNotifier> remote_push_notifier_;
    std::unique_ptr<GatewayPushDeliveryService> gateway_push_delivery_service_;
    std::unique_ptr<::grpc::Server> gateway_push_delivery_server_;
    int gateway_push_delivery_selected_port_ = 0;
#endif

};



}  // namespace gateway
}  // namespace im

#endif  // GATEWAY_SERVER_HPP
