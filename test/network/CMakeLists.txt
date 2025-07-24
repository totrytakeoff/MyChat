cmake_minimum_required(VERSION 3.10)
project(network_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 vcpkg 工具链（确保在所有 find_package 之前）
set(CMAKE_TOOLCHAIN_FILE "/home/myself/vcpkg/scripts/buildsystems/vcpkg.cmake" 
    CACHE STRING "Vcpkg toolchain file")

# 查找所需的库
find_package(Threads REQUIRED)
find_package(GTest REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# 添加测试源文件
set(TEST_SOURCES
    test_server.cpp
    test_session.cpp
    test_io_service_pool.cpp
)

# 添加网络库源文件
set(NETWORK_SOURCES
    ../../common/network/tcp_server.cpp
    ../../common/network/tcp_session.cpp
    ../../common/network/IOService_pool.cpp
    ../../common/utils/log_manager.cpp
    ../../common/utils/thread_pool.cpp
)

# 创建测试可执行文件
add_executable(network_tests ${TEST_SOURCES} ${NETWORK_SOURCES})

# 链接所需的库
target_link_libraries(network_tests 
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    Boost::system
    spdlog::spdlog
    stdc++fs
)

# 包含头文件目录
target_include_directories(network_tests PRIVATE 
    ../../common
    ../../common/network
    ../../common/utils
)

# 启用C++17
target_compile_features(network_tests PRIVATE cxx_std_17)

# 添加测试
add_test(NAME NetworkTests COMMAND network_tests)