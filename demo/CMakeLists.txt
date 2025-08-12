cmake_minimum_required(VERSION 3.10)
project(redis_demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_path(REDIS_PLUS_PLUS_HEADER sw)
find_library(REDIS_PLUS_PLUS_LIB redis++)
find_path(HIREDIS_HEADER hiredis)
find_library(HIREDIS_LIB hiredis)

if(NOT REDIS_PLUS_PLUS_HEADER)
    message(FATAL_ERROR "redis-plus-plus header not found")
endif()

if(NOT REDIS_PLUS_PLUS_LIB)
    message(FATAL_ERROR "redis-plus-plus library not found")
endif()

if(NOT HIREDIS_LIB)
    message(FATAL_ERROR "hiredis library not found")
endif()

# 创建可执行文件
add_executable(redis_basic_demo redis_basic_demo.cpp)

# 链接库
target_link_libraries(redis_basic_demo 
    ${REDIS_PLUS_PLUS_LIB}
    ${HIREDIS_LIB}
    pthread
)

# 包含目录
target_include_directories(redis_basic_demo PRIVATE 
    ${REDIS_PLUS_PLUS_HEADER}
    ${HIREDIS_HEADER}
)

# 编译选项
target_compile_options(redis_basic_demo PRIVATE -Wall -Wextra)

message(STATUS "Redis++ header: ${REDIS_PLUS_PLUS_HEADER}")
message(STATUS "Redis++ library: ${REDIS_PLUS_PLUS_LIB}")
message(STATUS "Hiredis library: ${HIREDIS_LIB}")