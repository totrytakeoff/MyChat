cmake_minimum_required(VERSION 3.16)
project(RouterManagerTests)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

# 添加可执行文件
add_executable(test_router_manager
    test_router_manager.cpp
    ../../gateway/router/router.mgr.cpp
    ../../common/utils/log_manager.cpp
)

# 链接库
target_link_libraries(test_router_manager
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    pthread
)

# 包含头文件目录
target_include_directories(test_router_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../gateway
)

# 添加测试
enable_testing()

# 注册GTest测试
add_test(
    NAME RouterManagerTests
    COMMAND test_router_manager
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 设置测试属性
set_tests_properties(RouterManagerTests PROPERTIES
    TIMEOUT 60
    LABELS "unit"
)

# 添加自定义目标来运行测试
add_custom_target(run_router_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS test_router_manager
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running router manager tests"
)

# 代码覆盖率支持 (可选)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(test_router_manager PRIVATE --coverage)
        target_link_options(test_router_manager PRIVATE --coverage)
    endif()
endif()

# 安装测试可执行文件 (可选)
install(TARGETS test_router_manager
    RUNTIME DESTINATION bin/tests
    COMPONENT tests
)

# 打印配置信息
message(STATUS "RouterManager Tests Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  GTest Found: ${GTest_FOUND}")
message(STATUS "  nlohmann_json Found: ${nlohmann_json_FOUND}")
message(STATUS "  spdlog Found: ${spdlog_FOUND}")