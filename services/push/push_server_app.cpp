#include "push_server_app.hpp"

#include <chrono>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>

#include "../../common/utils/log_manager.hpp"

namespace im::service::push {

PushServerApp::PushServerApp(PushServerConfig config)
    : config_(std::move(config))
{
    im::utils::LogManager::SetLogToFile("push_server", "logs/push_server.log");
    logger_ = im::utils::LogManager::GetLogger("push_server");

    const auto timeout = std::chrono::milliseconds(config_.timeout_ms);
    if (!config_.gateway_delivery_endpoint.empty()) {
        session_provider_ =
            std::make_unique<RemoteGatewayPushSessionProvider>(
                config_.gateway_delivery_endpoint, timeout);
        payload_sender_ =
            std::make_unique<RemoteGatewayPushPayloadSender>(
                config_.gateway_delivery_endpoint, timeout);
        delivery_marker_ =
            std::make_unique<RemoteGatewayPushDeliveryMarker>(
                config_.gateway_delivery_endpoint, timeout);
        logger_->info("Push server will use Gateway delivery endpoint {}",
                      config_.gateway_delivery_endpoint);
    } else {
        session_provider_ = std::make_unique<EmptyPushSessionProvider>();
        payload_sender_ = std::make_unique<NoopPushPayloadSender>();
        delivery_marker_ = std::make_unique<NoopPushDeliveryMarker>();
        logger_->warn("Push server started without Gateway delivery endpoint; "
                      "NotifyUser RPCs will use no-session/no-op adapters");
    }

    runtime_ = std::make_unique<PushRuntime>(
        session_provider_.get(), payload_sender_.get(), delivery_marker_.get());
    grpc_service_ = std::make_unique<PushGrpcService>(runtime_.get());
}

PushServerApp::~PushServerApp() {
    shutdown();
}

bool PushServerApp::start() {
    if (server_) {
        logger_->warn("Push server already running on {}", config_.listen_address);
        return true;
    }

    if (config_.require_gateway_delivery_endpoint &&
        config_.gateway_delivery_endpoint.empty()) {
        logger_->error("Push server requires push.gateway_delivery_endpoint, "
                       "but it is empty");
        return false;
    }

    ::grpc::ServerBuilder builder;
    int selected_port = 0;
    builder.AddListeningPort(config_.listen_address,
                             ::grpc::InsecureServerCredentials(),
                             &selected_port);
    builder.RegisterService(grpc_service_.get());

    server_ = builder.BuildAndStart();
    if (!server_) {
        logger_->error("Failed to start Push gRPC server on {}", config_.listen_address);
        return false;
    }

    selected_port_ = selected_port;
    logger_->info("Push gRPC server listening on {} (selected port {})",
                  config_.listen_address, selected_port_);
    return true;
}

void PushServerApp::wait() {
    if (server_) {
        server_->Wait();
    }
}

void PushServerApp::shutdown() {
    if (server_) {
        logger_->info("Stopping Push gRPC server on {}", config_.listen_address);
        server_->Shutdown();
        server_.reset();
        selected_port_ = 0;
    }
}

} // namespace im::service::push
