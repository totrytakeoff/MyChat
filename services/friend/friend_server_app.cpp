#include "friend_server_app.hpp"

#include <exception>
#include <memory>
#include <utility>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <odb/pgsql/database.hxx>

#include "../../common/utils/log_manager.hpp"
#include "../user/password_hasher.hpp"
#include "../user/user_service.hpp"

namespace im::service::friend_ {

FriendServerApp::FriendServerApp(FriendServerConfig config)
    : config_(std::move(config)) {
    im::utils::LogManager::SetLogToFile("friend_server", "logs/friend_server.log");
    logger_ = im::utils::LogManager::GetLogger("friend_server");

    if (!config_.postgres_connection_string.empty()) {
        db_ = std::make_shared<odb::pgsql::database>(config_.postgres_connection_string);
        user_service_ = std::make_shared<im::service::user::UserService>(
            db_, std::make_unique<im::service::user::PasswordHasher>());
        friend_service_ = std::make_unique<FriendService>(db_, user_service_);
        grpc_service_ = std::make_unique<FriendGrpcService>(friend_service_.get());
    }
}

FriendServerApp::~FriendServerApp() {
    shutdown();
}

bool FriendServerApp::start() {
    if (server_) {
        logger_->warn("Friend server already running on {}", config_.listen_address);
        return true;
    }

    if (!grpc_service_) {
        logger_->error("Friend server requires a PostgreSQL connection string");
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
        logger_->error("Failed to start Friend gRPC server on {}", config_.listen_address);
        return false;
    }

    selected_port_ = selected_port;
    logger_->info("Friend gRPC server listening on {} (selected port {})",
                  config_.listen_address, selected_port_);
    return true;
}

void FriendServerApp::wait() {
    if (server_) {
        server_->Wait();
    }
}

void FriendServerApp::shutdown() {
    if (server_) {
        logger_->info("Stopping Friend gRPC server on {}", config_.listen_address);
        server_->Shutdown();
        server_.reset();
        selected_port_ = 0;
    }
}

}  // namespace im::service::friend_
