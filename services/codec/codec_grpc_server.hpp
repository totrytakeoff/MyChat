#ifndef CODEC_GRPC_SERVER_HPP
#define CODEC_GRPC_SERVER_HPP

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include "network/codec_service_impl.hpp"

namespace im {
namespace network {

class CodecGrpcServer {
public:
    CodecGrpcServer(const std::string& address);
    
    void run();
    void shutdown();

private:
    std::string address_;
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<CodecServiceImpl> service_;
};

} // namespace network
} // namespace im

#endif // CODEC_GRPC_SERVER_HPP