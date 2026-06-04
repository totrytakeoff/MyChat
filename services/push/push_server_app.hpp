#ifndef SERVICES_PUSH_PUSH_SERVER_APP_HPP
#define SERVICES_PUSH_PUSH_SERVER_APP_HPP

#include <memory>
#include <string>

#include <grpcpp/server.h>
#include <spdlog/logger.h>

#include "push_grpc_service.hpp"
#include "push_runtime.hpp"
#include "push_server_adapters.hpp"

namespace im::service::push {

struct PushServerConfig {
    std::string listen_address = "0.0.0.0:9101";
};

class PushServerApp {
public:
    explicit PushServerApp(PushServerConfig config = {});
    ~PushServerApp();

    PushServerApp(const PushServerApp&) = delete;
    PushServerApp& operator=(const PushServerApp&) = delete;

    bool start();
    void wait();
    void shutdown();

    const std::string& listen_address() const { return config_.listen_address; }
    int selected_port() const { return selected_port_; }
    bool is_running() const { return static_cast<bool>(server_); }

private:
    PushServerConfig config_;
    EmptyPushSessionProvider session_provider_;
    NoopPushPayloadSender payload_sender_;
    NoopPushDeliveryMarker delivery_marker_;
    PushRuntime runtime_;
    PushGrpcService grpc_service_;
    std::unique_ptr<::grpc::Server> server_;
    std::shared_ptr<spdlog::logger> logger_;
    int selected_port_ = 0;
};

} // namespace im::service::push

#endif // SERVICES_PUSH_PUSH_SERVER_APP_HPP
