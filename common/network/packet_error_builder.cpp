#include "packet_error_builder.hpp"

#include <nlohmann/json.hpp>

#include "protobuf_codec.hpp"
#include "../utils/log_manager.hpp"

namespace im::network {

std::string PacketErrorBuilder::build_json_error_response(
        const im::base::IMHeader& request_header,
        im::base::ErrorCode error_code,
        const std::string& error_message,
        const std::string& device_id,
        const std::string& platform)
{
    const im::base::IMHeader response_header =
            ProtobufCodec::returnHeaderBuilder(request_header, device_id, platform);

    const nlohmann::json body = {
            {"code", static_cast<int>(error_code)},
            {"body", ""},
            {"err_msg", error_message},
    };

    std::string encoded;
    if (ProtobufCodec::encodeEnvelope(response_header, kJsonTypeName, body.dump(), encoded)) {
        return encoded;
    }

    im::utils::LogManager::GetLogger("packet_error_builder")
            ->error("Failed to encode JSON packet error response: {}", error_message);
    return "";
}

} // namespace im::network
