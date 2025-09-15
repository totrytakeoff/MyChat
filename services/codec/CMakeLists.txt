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

# 生成的proto代码
set(CODEC_PROTO_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/proto/codec_service.pb.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/proto/codec_service.grpc.pb.cc
)

# 服务实现代码
set(CODEC_SERVICE_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/codec_service_impl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/codec_grpc_server.cpp
)

# 创建可执行文件
add_executable(codec_service 
  ${CODEC_PROTO_SRCS}
  ${CODEC_SERVICE_SRCS}
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# 链接库
target_link_libraries(codec_service
  ${gRPC::grpc++}
  ${Protobuf_LIBRARIES}
  im_network
  im_utils
  im_base
  pthread
  dl
)

# 如果 im_network/im_utils/im_base 是通过 add_subdirectory 引入的 target，确保它们也被链接
# target_link_libraries(codec_service PRIVATE im_network im_utils im_base)
# 编译选项
add_compile_options(-Wall -Wextra -Wpedantic)

# 安装目标
install(TARGETS codec_service
  RUNTIME DESTINATION bin)
