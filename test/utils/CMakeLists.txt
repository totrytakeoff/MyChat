cmake_minimum_required(VERSION 3.10)
project(utils_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 vcpkg 工具链（确保在所有 find_package 之前）
set(CMAKE_TOOLCHAIN_FILE "/home/myself/vcpkg/scripts/buildsystems/vcpkg.cmake" 
    CACHE STRING "Vcpkg toolchain file")

# 分别查找包
find_package(GTest CONFIG REQUIRED)          # 查找 GTest
find_package(nlohmann_json CONFIG REQUIRED)  # 查找 nlohmann-json
find_package(spdlog CONFIG REQUIRED)         # 查找 spdlog

# 包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/../../common/utils
)

# 测试可执行文件
add_executable(test_utils
    test_utils.cpp 
    ../../../common/utils/connection_pool.hpp 
    ../../../common/utils/log_manager.cpp
    ../../../common/utils/thread_pool.cpp
)

target_link_libraries(test_utils
    GTest::gtest_main
    pthread
    spdlog::spdlog
)

# 其他测试可执行文件
add_executable(test_memory_pool_perf test_memory_pool_perf.cpp)
target_link_libraries(test_memory_pool_perf 
    GTest::gtest_main 
    pthread
) 

# 配置测试可执行文件
add_executable(test_config 
    test_config_mgr.cpp 
    ../../../common/utils/config_mgr.hpp
)

target_link_libraries(test_config
    GTest::gtest_main 
    pthread 
    nlohmann_json::nlohmann_json  # 正确链接 nlohmann-json
)