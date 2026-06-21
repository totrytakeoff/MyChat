cmake_minimum_required(VERSION 3.16)
project(MyChatBenchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT PROJECT_ROOT)
    set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
endif()
message(STATUS "PROJECT_ROOT: ${PROJECT_ROOT}")

find_package(Boost REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(absl CONFIG QUIET)
find_package(utf8_range CONFIG QUIET)
find_package(PkgConfig QUIET)

set(MYCHAT_BENCH_EXTRA_PROTO_LIBS "")
if(TARGET absl::base)
    list(APPEND MYCHAT_BENCH_EXTRA_PROTO_LIBS
        absl::base
        absl::strings
        absl::str_format
        absl::time
        absl::synchronization
        absl::memory
        absl::status
        absl::statusor
        absl::log
        absl::log_internal_check_op
        absl::log_internal_message
        absl::cord
        absl::cord_internal
        absl::container_common
        absl::hash
        absl::raw_hash_set
    )
elseif(PkgConfig_FOUND)
    pkg_check_modules(ABSL QUIET IMPORTED_TARGET absl_strings)
    if(TARGET PkgConfig::ABSL)
        list(APPEND MYCHAT_BENCH_EXTRA_PROTO_LIBS PkgConfig::ABSL)
    endif()
endif()
if(TARGET utf8_range::utf8_range)
    list(APPEND MYCHAT_BENCH_EXTRA_PROTO_LIBS
        utf8_range::utf8_range
        utf8_range::utf8_validity
    )
endif()

option(MYCHAT_BENCH_REGENERATE_PROTO
    "Regenerate benchmark protobuf sources with the active protoc" OFF)

set(MYCHAT_BENCH_PROTO_NAMES base command user message)
if(MYCHAT_BENCH_REGENERATE_PROTO)
    set(MYCHAT_BENCH_PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(MYCHAT_BENCH_PROTO_FLAT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_flat")
    set(MYCHAT_BENCH_PROTO_SRCS "")
    foreach(proto_name IN LISTS MYCHAT_BENCH_PROTO_NAMES)
        set(proto_file "${PROJECT_ROOT}/common/proto/${proto_name}.proto")
        set(flat_cc "${MYCHAT_BENCH_PROTO_FLAT_DIR}/${proto_name}.pb.cc")
        set(flat_h "${MYCHAT_BENCH_PROTO_FLAT_DIR}/${proto_name}.pb.h")
        set(out_cc "${MYCHAT_BENCH_PROTO_GEN_DIR}/common/proto/${proto_name}.pb.cc")
        set(out_h "${MYCHAT_BENCH_PROTO_GEN_DIR}/common/proto/${proto_name}.pb.h")
        add_custom_command(
            OUTPUT "${out_cc}" "${out_h}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${MYCHAT_BENCH_PROTO_FLAT_DIR}"
                    "${MYCHAT_BENCH_PROTO_GEN_DIR}/common/proto"
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                    --proto_path="${PROJECT_ROOT}/common/proto"
                    --cpp_out="${MYCHAT_BENCH_PROTO_FLAT_DIR}"
                    "${proto_file}"
            COMMAND ${CMAKE_COMMAND} -E copy "${flat_cc}" "${out_cc}"
            COMMAND ${CMAKE_COMMAND} -E copy "${flat_h}" "${out_h}"
            DEPENDS "${proto_file}"
            COMMENT "Generating benchmark protobuf source for ${proto_name}.proto")
        list(APPEND MYCHAT_BENCH_PROTO_SRCS "${out_cc}")
    endforeach()
else()
    set(MYCHAT_BENCH_PROTO_GEN_DIR "")
    set(MYCHAT_BENCH_PROTO_SRCS
        "${PROJECT_ROOT}/common/proto/base.pb.cc"
        "${PROJECT_ROOT}/common/proto/command.pb.cc"
        "${PROJECT_ROOT}/common/proto/user.pb.cc"
        "${PROJECT_ROOT}/common/proto/message.pb.cc"
    )
endif()

add_executable(bench_ws
    bench_ws.cpp
    benchmark_codec.cpp
    ${MYCHAT_BENCH_PROTO_SRCS}
)

target_include_directories(bench_ws PRIVATE
    "${MYCHAT_BENCH_PROTO_GEN_DIR}"
    "${PROJECT_ROOT}"
    "${PROJECT_ROOT}/common"
    "${PROJECT_ROOT}/common/network"
    "${PROJECT_ROOT}/common/utils"
    "${PROJECT_ROOT}/common/proto"
    ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(bench_ws PRIVATE
    ${Protobuf_LIBRARIES}
    Boost::boost
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ${MYCHAT_BENCH_EXTRA_PROTO_LIBS}
)

set_target_properties(bench_ws PROPERTIES
    LINK_FLAGS "-static-libstdc++ -static-libgcc -s"
)
