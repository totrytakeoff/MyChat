#include "network/codec_grpc_server.hpp"
#include "utils/log_manager.hpp"

namespace im {
namespace network {

CodecGrpcServer::CodecGrpcServer(const std::string& address) 
    : address_(address) {
}

void CodecGrpcServer::run() {
    auto logger = LogManager::GetLogger("codec_grpc_server");
    
    try {
        service_ = std::make_unique<CodecServiceImpl>();
        
        grpc::ServerBuilder builder;
        builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
        builder.RegisterService(service_.get());
        
        builder.SetMaxMessageSize(64 << 20); // 64MB
        
        server_ = builder.BuildAndStart();
        
        if (server_) {
            logger->info("Codec gRPC server listening on {}", address_);
            server_->Wait();
        } else {
            logger->error("Failed to start Codec gRPC server on {}", address_);
        }
    } catch (const std::exception& e) {
        logger->error("Exception occurred while starting server: {}", e.what());
    } catch (...) {
        logger->error("Unknown exception occurred while starting server");
    }
}

void CodecGrpcServer::shutdown() {
    auto logger = LogManager::GetLogger("codec_grpc_server");
    
    if (server_) {
        logger->info("Shutting down Codec gRPC server");
        server_->Shutdown();
    }
}

} // namespace network
} // namespace im