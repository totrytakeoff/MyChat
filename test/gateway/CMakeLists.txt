cmake_minimum_required(VERSION 3.14)
project(gateway_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置vcpkg工具链
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# 查找包
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(jwt-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(httplib REQUIRED)
# Ensure we use static OpenSSL libraries from vcpkg
set(OpenSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
find_package(redis++ REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/../..)
include_directories(${CMAKE_SOURCE_DIR}/../../common)
include_directories(${CMAKE_SOURCE_DIR}/../../gateway)
include_directories(${UUID_INCLUDE_DIRS})
include_directories(${Protobuf_INCLUDE_DIRS})

# 网关服务器源文件
set(GATEWAY_SERVER_SOURCES
    ${CMAKE_SOURCE_DIR}/../../gateway/gateway_server/gateway_server.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/connection_manager/connection_manager.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/auth/multi_platform_auth.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/router/router.mgr.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/message_parser.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/coro_message_processor.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/message_processor.cpp
)

# 公共组件源文件
set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/../../common/network/IOService_pool.cpp
    ${CMAKE_SOURCE_DIR}/../../common/network/tcp_server.cpp
    ${CMAKE_SOURCE_DIR}/../../common/network/tcp_session.cpp
    ${CMAKE_SOURCE_DIR}/../../common/network/websocket_server.cpp
    ${CMAKE_SOURCE_DIR}/../../common/network/websocket_session.cpp
    ${CMAKE_SOURCE_DIR}/../../common/network/protobuf_codec.cpp
    ${CMAKE_SOURCE_DIR}/../../common/utils/log_manager.cpp
    ${CMAKE_SOURCE_DIR}/../../common/utils/coroutine_manager.cpp
    ${CMAKE_SOURCE_DIR}/../../common/utils/service_identity.cpp
    ${CMAKE_SOURCE_DIR}/../../common/database/redis_mgr.cpp
)

# Protobuf源文件
set(PROTO_SOURCES
    ${CMAKE_SOURCE_DIR}/../../common/proto/base.pb.cc
    ${CMAKE_SOURCE_DIR}/../../common/proto/command.pb.cc
    ${CMAKE_SOURCE_DIR}/../../common/proto/user.pb.cc
    ${CMAKE_SOURCE_DIR}/../../common/proto/friend.pb.cc
)

# 测试可执行文件
add_executable(gateway_server_tests
    gateway_server_test.cpp
    ${GATEWAY_SERVER_SOURCES}
    ${COMMON_SOURCES}
    ${PROTO_SOURCES}
)

# 链接库
target_link_libraries(gateway_server_tests
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    Boost::system
    jwt-cpp::jwt-cpp
    nlohmann_json::nlohmann_json
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
    redis++::redis++_static
    spdlog::spdlog
    ${UUID_LIBRARIES}
    ${Protobuf_LIBRARIES}
)

# 编译选项
target_compile_options(gateway_server_tests PRIVATE
    -Wall
    -Wextra
    -pedantic
    -fcoroutines)