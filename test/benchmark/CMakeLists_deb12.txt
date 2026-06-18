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
find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(ABSL REQUIRED IMPORTED_TARGET absl_strings)

add_executable(bench_ws
    bench_ws.cpp
    "${PROJECT_ROOT}/common/network/protobuf_codec.cpp"
    "${PROJECT_ROOT}/common/utils/log_manager.cpp"
    "${PROJECT_ROOT}/common/proto/base.pb.cc"
    "${PROJECT_ROOT}/common/proto/command.pb.cc"
    "${PROJECT_ROOT}/common/proto/user.pb.cc"
    "${PROJECT_ROOT}/common/proto/message.pb.cc"
)

target_include_directories(bench_ws PRIVATE
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
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    PkgConfig::ABSL
)

set_target_properties(bench_ws PROPERTIES
    LINK_FLAGS "-static-libstdc++ -static-libgcc -s"
)
