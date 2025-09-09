cmake_minimum_required(VERSION 3.16)
project(MyChat_Tests)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找vcpkg包
find_package(GTest CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(PkgConfig REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Redis依赖（通过vcpkg）
find_package(hiredis CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -DDEBUG")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")
endif()

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR}/../common)
include_directories(${CMAKE_SOURCE_DIR}/../gateway)

# 添加protobuf生成的文件
file(GLOB PROTO_SRCS 
    "${CMAKE_SOURCE_DIR}/../common/proto/*.pb.cc"
)

# 添加通用源文件
file(GLOB COMMON_SRCS
    "${CMAKE_SOURCE_DIR}/../common/network/*.cpp"
    "${CMAKE_SOURCE_DIR}/../common/utils/*.cpp"
)

# 基础测试不需要gateway组件
set(GATEWAY_SRCS)

# 排除示例和测试文件
list(FILTER GATEWAY_SRCS EXCLUDE REGEX ".*example.*")
list(FILTER GATEWAY_SRCS EXCLUDE REGEX ".*test.*")

# 基础组件测试可执行文件
add_executable(basic_tests
    simple_test.cpp
    ${PROTO_SRCS}
    ${COMMON_SRCS}
)

# 链接库
target_link_libraries(basic_tests
    GTest::gtest
    GTest::gtest_main
    protobuf::libprotobuf
    Boost::system
    Boost::filesystem
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    pthread
)

# Redis支持
target_link_libraries(basic_tests
    hiredis::hiredis
    redis++::redis++_static
)

# 可选的SSL支持
find_package(OpenSSL)
if(OpenSSL_FOUND)
    target_link_libraries(basic_tests
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_compile_definitions(basic_tests PRIVATE HAVE_SSL=1)
endif()

# 添加测试
include(GoogleTest)
gtest_discover_tests(basic_tests)

# 复制配置文件到构建目录
configure_file(${CMAKE_SOURCE_DIR}/config/test_platform_strategy.json 
               ${CMAKE_BINARY_DIR}/config/test_platform_strategy.json COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/test_router_config.json 
               ${CMAKE_BINARY_DIR}/config/test_router_config.json COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/config/test_auth_config.json 
               ${CMAKE_BINARY_DIR}/config/test_auth_config.json COPYONLY)