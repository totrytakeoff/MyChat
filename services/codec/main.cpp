#include <memory>
#include <iostream>
#include "network/codec_grpc_server.hpp"

int main(int argc, char** argv) {
    // 设置日志系统
    im::utils::LogManager::Initialize();
    
    // 创建并运行gRPC服务器
    std::string server_address("0.0.0.0:50051");
    im::network::CodecGrpcServer server(server_address);
    
    std::cout << "Starting Codec gRPC server on " << server_address << std::endl;
    server.run();
    
    return 0;
}