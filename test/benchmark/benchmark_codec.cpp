#include "benchmark_codec.hpp"

#include <cstring>

namespace im::benchmark {
namespace {

void writeVarint32(uint32_t value, std::string& output) {
    while (value > 0x7F) {
        output.push_back(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    output.push_back(static_cast<char>(value));
}

bool readVarint32(const std::string& input, size_t limit, size_t& offset, uint32_t& value) {
    value = 0;
    uint32_t shift = 0;
    while (offset < limit && shift <= 28) {
        const auto byte = static_cast<unsigned char>(input[offset++]);
        value |= static_cast<uint32_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }
    return false;
}

}  // namespace

bool BenchmarkCodec::encode(const im::base::IMHeader& header,
                            const google::protobuf::Message& message,
                            std::string& output) {
    std::string header_data;
    std::string message_data;
    if (!header.SerializeToString(&header_data) || !message.SerializeToString(&message_data)) {
        return false;
    }

    const std::string type_name = message.GetTypeName();
    output.clear();
    output.reserve(header_data.size() + message_data.size() + type_name.size() + 20 + 4);

    writeVarint32(static_cast<uint32_t>(header_data.size()), output);
    writeVarint32(static_cast<uint32_t>(type_name.size()), output);
    output.append(type_name);
    output.append(header_data);
    output.append(message_data);

    const uint32_t crc = calculateCRC32(output.data(), output.size());
    output.append(reinterpret_cast<const char*>(&crc), sizeof(crc));
    return true;
}

bool BenchmarkCodec::decodeEnvelope(const std::string& input,
                                    im::base::IMHeader& header_out,
                                    std::string& type_name_out,
                                    std::string& message_bytes_out) {
    if (input.size() < sizeof(uint32_t)) {
        return false;
    }

    uint32_t received_crc = 0;
    std::memcpy(&received_crc, input.data() + input.size() - sizeof(received_crc),
                sizeof(received_crc));
    const size_t payload_limit = input.size() - sizeof(received_crc);
    const uint32_t calculated_crc = calculateCRC32(input.data(), payload_limit);
    if (received_crc != calculated_crc) {
        return false;
    }

    size_t offset = 0;
    uint32_t header_size = 0;
    uint32_t type_name_size = 0;
    if (!readVarint32(input, payload_limit, offset, header_size) ||
        !readVarint32(input, payload_limit, offset, type_name_size)) {
        return false;
    }
    if (header_size == 0 || type_name_size == 0 ||
        offset + type_name_size + header_size > payload_limit) {
        return false;
    }

    type_name_out.assign(input.data() + offset, type_name_size);
    offset += type_name_size;

    if (!header_out.ParseFromArray(input.data() + offset, static_cast<int>(header_size))) {
        return false;
    }
    offset += header_size;

    message_bytes_out.assign(input.data() + offset, payload_limit - offset);
    return true;
}

uint32_t BenchmarkCodec::calculateCRC32(const void* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

}  // namespace im::benchmark
