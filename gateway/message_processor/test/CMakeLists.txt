# 消息处理器测试编译配置

cmake_minimum_required(VERSION 3.16)

# 查找必需的包
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)

# 添加测试可执行文件
add_executable(test_message_parser
    test_message_parser.cpp
    ../message_processor.cpp
)

# 包含头文件路径
target_include_directories(test_message_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..  # 包含common目录
    ${Protobuf_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(test_message_parser
    ${Protobuf_LIBRARIES}
    protobuf::libprotobuf
)

# 设置C++标准
target_compile_features(test_message_parser PRIVATE cxx_std_17)

# 编译选项
target_compile_options(test_message_parser PRIVATE
    -Wall
    -Wextra
    -O2
)