cmake_minimum_required(VERSION 3.10)
project(connection_manager_simple_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 vcpkg 工具链
set(CMAKE_TOOLCHAIN_FILE "/home/myself/vcpkg/scripts/buildsystems/vcpkg.cmake" 
    CACHE STRING "Vcpkg toolchain file")

# 查找依赖包
find_package(GTest CONFIG REQUIRED)          # 查找 GTest
find_package(nlohmann_json CONFIG REQUIRED)  # 查找 nlohmann-json
find_package(spdlog CONFIG REQUIRED)         # 查找 spdlog

# 包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/../../../common
    ${PROJECT_SOURCE_DIR}/../../../
)

# 创建最小化测试可执行文件（无外部依赖）
add_executable(minimal_test
    minimal_test.cpp
)

# 创建集成测试可执行文件
add_executable(integration_test
    integration_test.cpp
)

# 链接库
target_link_libraries(minimal_test
    GTest::gtest_main
    pthread
    nlohmann_json::nlohmann_json
)

target_link_libraries(integration_test
    GTest::gtest_main
    pthread
    nlohmann_json::nlohmann_json
)

# 设置编译选项
target_compile_definitions(minimal_test PRIVATE
    TESTING=1
)

target_compile_definitions(integration_test PRIVATE
    TESTING=1
)

# 启用测试
enable_testing()
add_test(NAME MinimalTest COMMAND minimal_test)
add_test(NAME IntegrationTest COMMAND integration_test)