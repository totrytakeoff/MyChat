# test/gateway/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(gateway_tests)

# 继承父项目的vcpkg配置
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-linux" ${CMAKE_PREFIX_PATH})
endif()

# 查找必要的依赖包
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread coroutine context filesystem)
find_package(spdlog CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

# 测试可执行文件
add_executable(gateway_server_tests_1
    gateway_server_test.cpp
    gateway_e2e_performance_test.cpp
)

target_link_libraries(gateway_server_tests_1
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core  # 链接预构建的核心库
        im::gateway_auth  # 链接预构建的认证库
)

# WebSocket测试可执行文件
add_executable(gateway_websocket_tests
    gateway_websocket_test.cpp
)

target_link_libraries(gateway_websocket_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core
        im::gateway_auth
)

# 手动服务器测试
add_executable(gateway_manual_server
    manual_server.cpp
)

target_link_libraries(gateway_manual_server
    PRIVATE
        im::gateway_core
        im::gateway_auth
)



# 添加测试目标
include(CTest)
add_test(NAME GatewayServerTests COMMAND gateway_server_tests_1)
add_test(NAME GatewayWebSocketTests COMMAND gateway_websocket_tests)
