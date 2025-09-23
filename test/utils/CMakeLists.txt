cmake_minimum_required(VERSION 3.10)
project(utils_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")

# 查找 Google Test
find_package(GTest REQUIRED)

# 包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/../../common/utils
    ${GTEST_INCLUDE_DIRS}
)

# 原有测试可执行文件
add_executable(minimal_coro_test
    minimal_coro_test.cpp
)

target_link_libraries(minimal_coro_test
    pthread
)

# 新的工具类测试

# SignalHandler 测试
add_executable(test_signal_handler
    test_signal_handler.cpp
)

target_link_libraries(test_signal_handler
    ${GTEST_LIBRARIES}
    pthread
)

# CLIParser 简化测试
add_executable(test_cli_parser_simple
    test_cli_parser_simple.cpp
)

target_link_libraries(test_cli_parser_simple
    ${GTEST_LIBRARIES}
    pthread
)

# ConfigManager 扩展功能测试
add_executable(test_config_mgr_extended
    test_config_mgr_extended.cpp
)

target_link_libraries(test_config_mgr_extended
    ${GTEST_LIBRARIES}
    im::utils
    pthread
)



# 添加测试到 CTest
enable_testing()

add_test(NAME SignalHandlerTest COMMAND test_signal_handler)
add_test(NAME CLIParserSimpleTest COMMAND test_cli_parser_simple)
add_test(NAME ConfigManagerExtendedTest COMMAND test_config_mgr_extended)
add_test(NAME UtilsIntegrationTest COMMAND test_utils_integration)

# 设置测试属性
set_tests_properties(SignalHandlerTest PROPERTIES TIMEOUT 30)
set_tests_properties(CLIParserSimpleTest PROPERTIES TIMEOUT 30)
set_tests_properties(ConfigManagerExtendedTest PROPERTIES TIMEOUT 30)
set_tests_properties(UtilsIntegrationTest PROPERTIES TIMEOUT 60)