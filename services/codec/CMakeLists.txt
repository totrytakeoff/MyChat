# CMakeLists.txt for Codec gRPC service

# 设置构建类型为C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 项目名称
project(codec_service)

# 查找必要的包
find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)

# 包含目录
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/proto
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common/proto
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common/network
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common/utils
  ${Protobuf_INCLUDE_DIRS}
  ${gRPC_INCLUDE_DIRS}
)

# 收集源文件 - 只包含本服务的源文件
set(SOURCES
    main.cpp
    codec_service_impl.cpp
)

# 创建可执行文件
add_executable(codec_service ${SOURCES})

# 链接库
target_link_libraries(codec_service
    PRIVATE
        gRPC::grpc++
        gRPC::grpc++_reflection
        im::codec_service
        im::utils
        im::network
        Threads::Threads
)

# 如果 im_network/im_utils/im_base 是通过 add_subdirectory 引入的 target，确保它们也被链接
# target_link_libraries(codec_service PRIVATE im_network im_utils im_base)
# 编译选项
add_compile_options(-Wall -Wextra -Wpedantic)

# 安装目标
install(TARGETS codec_service
  RUNTIME DESTINATION bin)
