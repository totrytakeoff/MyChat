#ifndef CODEC_SERVICE_IMPL_HPP
#define CODEC_SERVICE_IMPL_HPP

#include <grpcpp/grpcpp.h>
#include "proto/codec_service.grpc.pb.h"
#include "common/network/protobuf_codec.hpp"

namespace im {
namespace network {

class CodecServiceImpl final : public im::codec::CodecService::Service {
public:
    grpc::Status Encode(grpc::ServerContext* context, 
                       const im::codec::EncodeRequest* request, 
                       im::codec::EncodeResponse* response) override;

    grpc::Status Decode(grpc::ServerContext* context, 
                       const im::codec::DecodeRequest* request, 
                       im::codec::DecodeResponse* response) override;

private:
    ProtobufCodec codec_;
};

} // namespace network
} // namespace im

#endif // CODEC_SERVICE_IMPL_HPP