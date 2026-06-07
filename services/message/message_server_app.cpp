#include "message_server_app.hpp"

#include <exception>
#include <utility>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <odb/pgsql/database.hxx>

#include "../../common/utils/log_manager.hpp"

namespace im::service::message {

MessageServerApp::MessageServerApp(MessageServerConfig config)
    : config_(std::move(config)) {
    im::utils::LogManager::SetLogToFile("message_server", "logs/message_server.log");
    logger_ = im::utils::LogManager::GetLogger("message_server");

    if (!config_.postgres_connection_string.empty()) {
        db_ = std::make_shared<odb::pgsql::database>(config_.postgres_connection_string);
        message_service_ = std::make_unique<MessageService>(db_);
        grpc_service_ = std::make_unique<MessageGrpcService>(message_service_.get());
    }
}

MessageServerApp::~MessageServerApp() {
    shutdown();
}

bool MessageServerApp::start() {
    if (server_) {
        logger_->warn("Message server already running on {}", config_.listen_address);
        return true;
    }

    if (!grpc_service_) {
        logger_->error("Message server requires a PostgreSQL connection string");
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
        logger_->error("Failed to start Message gRPC server on {}", config_.listen_address);
        return false;
    }

    selected_port_ = selected_port;
    logger_->info("Message gRPC server listening on {} (selected port {})",
                  config_.listen_address, selected_port_);
    return true;
}

void MessageServerApp::wait() {
    if (server_) {
        server_->Wait();
    }
}

void MessageServerApp::shutdown() {
    if (server_) {
        logger_->info("Stopping Message gRPC server on {}", config_.listen_address);
        server_->Shutdown();
        server_.reset();
        selected_port_ = 0;
    }
}

}  // namespace im::service::message
