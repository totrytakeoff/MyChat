cmake_minimum_required(VERSION 3.16)

# 项目信息
project(MyChat VERSION 1.0.0 LANGUAGES CXX)

# C++标准设置
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置vcpkg工具链文件（如果环境变量存在）
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# 设置Boost根目录
set(Boost_ROOT "$ENV{VCPKG_ROOT}/installed/x64-linux")
set(Boost_USE_STATIC_LIBS ON)

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# 编译选项
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 查找依赖包（全局查找，所有子目录继承）
find_package(Threads REQUIRED)
# 使用vcpkg配置查找Boost
find_package(Boost REQUIRED COMPONENTS system thread coroutine context filesystem)
find_package(spdlog CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(utf8_range CONFIG REQUIRED)
find_package(unofficial-libuuid CONFIG REQUIRED)

# 将 absl 所有常用组件作为变量定义，便于子项目使用
set(ABSL_LIBRARIES
    absl::base
    absl::strings
    absl::str_format
    absl::time
    absl::synchronization
    absl::memory
    absl::status
    absl::statusor
    absl::log
    absl::log_internal_check_op
    absl::log_internal_message
    absl::container_common
    absl::hash
    absl::raw_hash_set
)

# 包含目录（全局包含，所有子目录继承）
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/network)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/utils)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/database)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/proto)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/gateway)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/services)

# 添加子目录
add_subdirectory(common)
add_subdirectory(gateway)
add_subdirectory(services)

# 可选的添加 codec 子服务
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/services/codec/CMakeLists.txt")
    add_subdirectory(services/codec)
endif()

# 启用测试
enable_testing()
add_subdirectory(test)