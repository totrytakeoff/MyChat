cmake_minimum_required(VERSION 3.16)
project(MessageProcessorTests)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置vcpkg工具链文件
set(CMAKE_TOOLCHAIN_FILE "/home/myself/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 查找必需的包 (通过vcpkg安装)
find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../common)

# 源文件列表
set(MESSAGE_PROCESSOR_SOURCES
    ../message_parser.cpp
    ../coro_message_processor.cpp
    ../../../common/network/protobuf_codec.cpp
    ../../../common/utils/log_manager.cpp
    ../../router/router.mgr.cpp
    ../../../common/proto/base.pb.cc
)

# 测试源文件
set(TEST_SOURCES
    test_message_parser.cpp
    mock_websocket_client.cpp
    mock_http_client.cpp
)

# 添加可执行文件
add_executable(test_message_processor
    ${TEST_SOURCES}
    ${MESSAGE_PROCESSOR_SOURCES}
)

# 添加简单的测试程序
add_executable(simple_test
    simple_test.cpp
    ../coro_message_processor.cpp
    ../message_parser.cpp
    ../../../common/network/protobuf_codec.cpp
    ../../../common/utils/log_manager.cpp
    ../../../common/utils/coroutine_manager.cpp
    ../../router/router.mgr.cpp
    ../../../common/proto/base.pb.cc
)

# 链接库
target_link_libraries(test_message_processor
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    httplib::httplib
    protobuf::libprotobuf
    pthread
)

# 包含头文件目录
target_include_directories(test_message_processor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../gateway
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 添加测试
enable_testing()

# 注册GTest测试
add_test(
    NAME MessageProcessorTests
    COMMAND test_message_processor
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 设置测试属性
set_tests_properties(MessageProcessorTests PROPERTIES
    TIMEOUT 120
    LABELS "unit"
)

# 添加自定义目标来运行测试
add_custom_target(run_message_processor_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS test_message_processor
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running message processor tests"
)

# 代码覆盖率支持 (可选)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(test_message_processor PRIVATE --coverage)
        target_link_options(test_message_processor PRIVATE --coverage)
    endif()
endif()

# 安装测试可执行文件 (可选)
install(TARGETS test_message_processor
    RUNTIME DESTINATION bin/tests
    COMPONENT tests
)

# 创建测试配置目录
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/config)

# 复制测试配置文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_router_config.json
    ${CMAKE_CURRENT_BINARY_DIR}/test_router_config.json
    COPYONLY
)

# 打印配置信息
message(STATUS "MessageProcessor Tests Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  GTest Found: ${GTest_FOUND}")
message(STATUS "  nlohmann_json Found: ${nlohmann_json_FOUND}")
message(STATUS "  spdlog Found: ${spdlog_FOUND}")
message(STATUS "  httplib Found: ${httplib_FOUND}")
message(STATUS "  Protobuf Found: ${Protobuf_FOUND}")