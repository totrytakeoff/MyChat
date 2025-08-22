# HttpRouter测试编译配置

cmake_minimum_required(VERSION 3.16)

# 查找必需的包
find_package(nlohmann_json REQUIRED)

# 添加测试可执行文件
add_executable(test_http_router
    test_http_router.cpp
    ../http_router.cpp
)

# 包含头文件路径
target_include_directories(test_http_router PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..  # 包含common目录
    ${nlohmann_json_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(test_http_router
    nlohmann_json::nlohmann_json
    pthread
)

# 设置C++标准
target_compile_features(test_http_router PRIVATE cxx_std_17)

# 编译选项
target_compile_options(test_http_router PRIVATE
    -Wall
    -Wextra
    -O2
)

# 添加测试目标
enable_testing()
add_test(NAME http_router_test COMMAND test_http_router)