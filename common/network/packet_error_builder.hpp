#ifndef PACKET_ERROR_BUILDER_HPP
#define PACKET_ERROR_BUILDER_HPP

#include <string>

#include "../proto/base.pb.h"

namespace im::network {

class PacketErrorBuilder {
public:
    static constexpr const char* kJsonTypeName = "application/json";

    static std::string build_json_error_response(
            const im::base::IMHeader& request_header,
            im::base::ErrorCode error_code,
            const std::string& error_message,
            const std::string& device_id,
            const std::string& platform);
};

} // namespace im::network

#endif // PACKET_ERROR_BUILDER_HPP
