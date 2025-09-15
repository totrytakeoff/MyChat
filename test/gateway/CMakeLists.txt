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
find_package(Boost REQUIRED COMPONENTS system thread coroutine context filesystem)
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
find_package(absl REQUIRED)
find_package(utf8_range REQUIRED)

# find_package(PkgConfig REQUIRED)
# pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
find_package(unofficial-libuuid CONFIG REQUIRED)


# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/../..)
include_directories(${CMAKE_SOURCE_DIR}/../../common)
include_directories(${CMAKE_SOURCE_DIR}/../../gateway)
include_directories(${Protobuf_INCLUDE_DIRS})

# 网关服务器源文件
set(GATEWAY_SERVER_SOURCES
    ${CMAKE_SOURCE_DIR}/../../gateway/gateway_server/gateway_server-t1.cpp
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
    ${CMAKE_SOURCE_DIR}/../../common/utils/thread_pool.cpp
    ${CMAKE_SOURCE_DIR}/../../common/database/redis_mgr.cpp
)

# Protobuf源文件
set(PROTO_SOURCES
    ${CMAKE_SOURCE_DIR}/../../common/proto/base.pb.cc
    ${CMAKE_SOURCE_DIR}/../../common/proto/command.pb.cc
    ${CMAKE_SOURCE_DIR}/../../common/proto/user.pb.cc
    ${CMAKE_SOURCE_DIR}/../../common/proto/friend.pb.cc
)

# 测试可执行文件_0
# add_executable(gateway_server_tests
#     gateway_server_test.cpp
#     gateway_e2e_performance_test.cpp
#     ${GATEWAY_SERVER_SOURCES}
#     ${COMMON_SOURCES}
#     ${PROTO_SOURCES}
# )

#  测试可执行文件_1
add_executable(gateway_server_tests_1
    gateway_server_test.cpp
    gateway_e2e_performance_test.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/gateway_server/gateway_server-t1.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/connection_manager/connection_manager.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/auth/multi_platform_auth.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/router/router.mgr.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/message_parser.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/coro_message_processor.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/message_processor.cpp

    ${COMMON_SOURCES}
    ${PROTO_SOURCES}
)

add_executable(gateway_manual_server
    manual_server.cpp
    ${GATEWAY_SERVER_SOURCES}
    ${COMMON_SOURCES}
    ${PROTO_SOURCES}
)

target_link_libraries(gateway_manual_server
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
    # PkgConfig::UUID
    unofficial::UUID::uuid
    ${Protobuf_LIBRARIES}
    utf8_range::utf8_range
    utf8_range::utf8_validity
    absl::base
    absl::strings
    absl::str_format
    absl::time
    absl::synchronization
    absl::memory
    absl::status
    absl::statusor
    absl::log
    absl::raw_logging_internal
    absl::log_internal_check_op
    absl::log_internal_message
    absl::log_internal_globals
    absl::log_internal_config
    absl::log_internal_flags
    absl::log_internal_log_sink_set
    absl::log_sink
    absl::log_entry
    absl::log_severity
    absl::log_internal_nullstream
    absl::log_internal_voidify
    absl::log_internal_append_truncated
    absl::log_internal_strip
    absl::log_internal_format
    absl::log_internal_proto
    absl::log_internal_structured
    absl::log_internal_structured_proto
    absl::log_globals
    absl::log_initialize
    absl::log_flags
    absl::log_streamer
    absl::log_structured
    absl::scoped_set_env
    absl::config
    absl::core_headers
    absl::dynamic_annotations
    absl::errno_saver
    absl::raw_logging_internal
    absl::throw_delegate
    absl::utility
    absl::type_traits
    absl::meta
)

# 链接库

# 链接库
target_link_libraries(gateway_server_tests_1
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
    # PkgConfig::UUID
    unofficial::UUID::uuid
    ${Protobuf_LIBRARIES}
    utf8_range::utf8_range
    utf8_range::utf8_validity
    absl::base
    absl::strings
    absl::str_format
    absl::time
    absl::synchronization
    absl::memory
    absl::status
    absl::statusor
    absl::log
    absl::raw_logging_internal
    absl::log_internal_check_op
    absl::log_internal_message
    absl::log_internal_globals
    absl::log_internal_config
    absl::log_internal_flags
    absl::log_internal_log_sink_set
    absl::log_sink
    absl::log_entry
    absl::log_severity
    absl::log_internal_nullstream
    absl::log_internal_voidify
    absl::log_internal_append_truncated
    absl::log_internal_strip
    absl::log_internal_format
    absl::log_internal_proto
    absl::log_internal_structured
    absl::log_internal_structured_proto
    absl::log_globals
    absl::log_initialize
    absl::log_flags
    absl::log_streamer
    absl::log_structured
    absl::scoped_set_env
    absl::config
    absl::core_headers
    absl::dynamic_annotations
    absl::errno_saver
    absl::raw_logging_internal
    absl::throw_delegate
    absl::utility
    absl::type_traits
    absl::meta
)

# 编译选项
target_compile_options(gateway_server_tests_1 PRIVATE
    -Wall
    -Wextra
    -pedantic
    -fcoroutines)

# WebSocket测试可执行文件（直接编译源码集合，与 gateway_server_tests_1 一致的方式）
add_executable(gateway_websocket_tests
    gateway_websocket_test.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/gateway_server/gateway_server-t1.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/connection_manager/connection_manager.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/auth/multi_platform_auth.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/router/router.mgr.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/message_parser.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/coro_message_processor.cpp
    ${CMAKE_SOURCE_DIR}/../../gateway/message_processor/message_processor.cpp
    ${COMMON_SOURCES}
    ${PROTO_SOURCES}
)

# WebSocket测试链接第三方库（复用 gateway_server_tests_1 的外部依赖）
target_link_libraries(gateway_websocket_tests
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
    unofficial::UUID::uuid
    ${Protobuf_LIBRARIES}
    utf8_range::utf8_range
    utf8_range::utf8_validity
    absl::base
    absl::strings
    absl::str_format
    absl::time
    absl::synchronization
    absl::memory
    absl::status
    absl::statusor
    absl::log
    absl::raw_logging_internal
    absl::log_internal_check_op
    absl::log_internal_message
    absl::log_internal_globals
    absl::log_internal_config
    absl::log_internal_flags
    absl::log_internal_log_sink_set
    absl::log_sink
    absl::log_entry
    absl::log_severity
    absl::log_internal_nullstream
    absl::log_internal_voidify
    absl::log_internal_append_truncated
    absl::log_internal_strip
    absl::log_internal_format
    absl::log_internal_proto
    absl::log_internal_structured
    absl::log_internal_structured_proto
    absl::log_globals
    absl::log_initialize
    absl::log_flags
    absl::log_streamer
    absl::log_structured
    absl::scoped_set_env
    absl::config
    absl::core_headers
    absl::dynamic_annotations
    absl::errno_saver
    absl::throw_delegate
    absl::utility
    absl::type_traits
    absl::meta
)

# WebSocket测试编译选项
target_compile_options(gateway_websocket_tests PRIVATE
    -Wall
    -Wextra
    -pedantic
    -fcoroutines)