#include "network/codec_service_impl.hpp"
#include "utils/log_manager.hpp"
#include <google/protobuf/descriptor.h>

namespace im {
namespace network {

using im::utils::LogManager;

grpc::Status CodecServiceImpl::Encode(grpc::ServerContext* context,
                                     const im::codec::EncodeRequest* request,
                                     im::codec::EncodeResponse* response) {
    try {
        auto logger = LogManager::GetLogger("codec_service");
        logger->debug("Encode request received");

        if (!request->has_header()) {
            response->set_success(false);
            response->set_error_message("Missing header in request");
            return grpc::Status::OK;
        }

        if (request->message_type_name().empty()) {
            response->set_success(false);
            response->set_error_message("Missing message type name");
            return grpc::Status::OK;
        }

        const google::protobuf::Descriptor* descriptor = 
            google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
                request->message_type_name());
        
        if (!descriptor) {
            response->set_success(false);
            response->set_error_message("Unknown message type: " + request->message_type_name());
            return grpc::Status::OK;
        }

        google::protobuf::Message* message = 
            google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor)->New();
        
        if (!message) {
            response->set_success(false);
            response->set_error_message("Failed to create message instance for type: " + 
                                      request->message_type_name());
            return grpc::Status::OK;
        }

        if (!message->ParseFromString(request->message_data())) {
            delete message;
            response->set_success(false);
            response->set_error_message("Failed to parse message data");
            return grpc::Status::OK;
        }

        std::string output;
        bool result = codec_.encode(request->header(), *message, output);

        delete message;

        if (result) {
            response->set_success(true);
            response->set_encoded_data(output);
            logger->debug("Encode successful, output size: {}", output.size());
        } else {
            response->set_success(false);
            response->set_error_message("Failed to encode message");
            logger->error("Encode failed");
        }

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_error_message(std::string("Exception occurred: ") + e.what());
        return grpc::Status::OK;
    } catch (...) {
        response->set_success(false);
        response->set_error_message("Unknown exception occurred");
        return grpc::Status::OK;
    }
}

grpc::Status CodecServiceImpl::Decode(grpc::ServerContext* context,
                                     const im::codec::DecodeRequest* request,
                                     im::codec::DecodeResponse* response) {
    try {
        auto logger = LogManager::GetLogger("codec_service");
        logger->debug("Decode request received, input size: {}", request->encoded_data().size());

        im::base::IMHeader header;
        google::protobuf::Message* message = nullptr;
        
        bool result = codec_.decode(request->encoded_data(), header, *message);

        if (result) {
            response->set_success(true);
            response->mutable_header()->CopyFrom(header);
            
            std::string message_data;
            if (message->SerializeToString(&message_data)) {
                response->set_message_data(message_data);
                response->set_message_type_name(message->GetTypeName());
                logger->debug("Decode successful, message type: {}", message->GetTypeName());
            } else {
                delete message;
                response->set_success(false);
                response->set_error_message("Failed to serialize decoded message");
                return grpc::Status::OK;
            }
        } else {
            response->set_success(false);
            response->set_error_message("Failed to decode message");
            logger->error("Decode failed");
        }

        delete message;
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_error_message(std::string("Exception occurred: ") + e.what());
        return grpc::Status::OK;
    } catch (...) {
        response->set_success(false);
        response->set_error_message("Unknown exception occurred");
        return grpc::Status::OK;
    }
}

} // namespace network
} // namespace im