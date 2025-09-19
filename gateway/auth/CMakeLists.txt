# gateway/auth/CMakeLists.txt

# 添加测试可执行文件
add_executable(test_multi_platform_auth
    test_multi_platform_auth.cpp
    multi_platform_auth.cpp
)

# 链接库（使用新的组件库）
target_link_libraries(test_multi_platform_auth
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_auth  # 使用新的认证库
        im::database      # 使用新的数据库库
        im::utils         # 使用新的工具库
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        redis++::redis++_static
        jwt-cpp::jwt-cpp
        unofficial::UUID::uuid
        Threads::Threads
)

# 编译选项
target_compile_options(test_multi_platform_auth PRIVATE
    -Wall -Wextra -g
)

# 包含目录
target_include_directories(test_multi_platform_auth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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