cmake_minimum_required(VERSION 3.10)
project(utils_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# gtest from apt
find_package(GTest REQUIRED)

# spdlog from vcpkg
set(CMAKE_TOOLCHAIN_FILE "/home/myself/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
find_package(spdlog CONFIG REQUIRED)

include_directories(
    ${PROJECT_SOURCE_DIR}/../../common/utils
)

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