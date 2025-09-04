cmake_minimum_required(VERSION 3.10)
project(minimal_coro_test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")

# 包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/../../common/utils
)

# 测试可执行文件
add_executable(minimal_coro_test
    minimal_coro_test.cpp
)

target_link_libraries(minimal_coro_test
    pthread
)