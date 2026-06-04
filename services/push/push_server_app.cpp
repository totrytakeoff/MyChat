#include "push_server_app.hpp"

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>

#include "../../common/utils/log_manager.hpp"

namespace im::service::push {

PushServerApp::PushServerApp(PushServerConfig config)
    : config_(std::move(config))
    , runtime_(&session_provider_, &payload_sender_, &delivery_marker_)
    , grpc_service_(&runtime_)
{
    im::utils::LogManager::SetLogToFile("push_server", "logs/push_server.log");
    logger_ = im::utils::LogManager::GetLogger("push_server");
}

PushServerApp::~PushServerApp() {
    shutdown();
}

bool PushServerApp::start() {
    if (server_) {
        logger_->warn("Push server already running on {}", config_.listen_address);
        return true;
    }

    ::grpc::ServerBuilder builder;
    int selected_port = 0;
    builder.AddListeningPort(config_.listen_address,
                             ::grpc::InsecureServerCredentials(),
                             &selected_port);
    builder.RegisterService(&grpc_service_);

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
