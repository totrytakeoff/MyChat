cmake_minimum_required(VERSION 3.15)
project(AsyncProcessorDemo)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# 查找必要的库
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)

# 添加可执行文件
add_executable(async_processor_demo async_processor_demo.cpp)

# 链接库
target_link_libraries(async_processor_demo 
    pthread
    ${Protobuf_LIBRARIES}
)

# 包含目录
target_include_directories(async_processor_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${Protobuf_INCLUDE_DIRS}
)

# 编译说明
message(STATUS "异步处理器Demo配置完成")
message(STATUS "编译命令: make async_processor_demo")
message(STATUS "运行命令: ./async_processor_demo")