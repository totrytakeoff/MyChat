#pragma once

#include <string>

#include <google/protobuf/message.h>

#include "common/proto/base.pb.h"

namespace im::benchmark {

class BenchmarkCodec {
public:
    static bool encode(const im::base::IMHeader& header,
                       const google::protobuf::Message& message,
                       std::string& output);

    static bool decodeEnvelope(const std::string& input,
                               im::base::IMHeader& header_out,
                               std::string& type_name_out,
                               std::string& message_bytes_out);

private:
    static uint32_t calculateCRC32(const void* data, size_t size);
};

}  // namespace im::benchmark
