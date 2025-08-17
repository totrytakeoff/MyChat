cmake_minimum_required(VERSION 3.16)
project(MyChat_Auth_Tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的包
find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)

# 使用vcpkg查找依赖
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(unofficial-libuuid CONFIG REQUIRED)

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
)

# 添加源文件
set(AUTH_SOURCES
    multi_platform_auth.cpp
    ../../common/database/redis_mgr.cpp
    ../../common/utils/log_manager.cpp
)

# 添加测试可执行文件
add_executable(test_multi_platform_auth
    test_multi_platform_auth.cpp
    ${AUTH_SOURCES}
)

# 链接库
target_link_libraries(test_multi_platform_auth
    PRIVATE
    GTest::GTest
    GTest::Main
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    redis++::redis++_static
    jwt-cpp::jwt-cpp
    unofficial::UUID::uuid
    pthread
)

# 编译选项
target_compile_options(test_multi_platform_auth PRIVATE
    -Wall -Wextra -g
)

# 添加测试
enable_testing()
add_test(NAME AuthTests COMMAND test_multi_platform_auth)

# 便捷目标
add_custom_target(run_tests
    COMMAND ${CMAKE_BINARY_DIR}/test_multi_platform_auth
    DEPENDS test_multi_platform_auth
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)