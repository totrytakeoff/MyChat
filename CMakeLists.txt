cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# 设置vcpkg工具链文件。优先使用环境变量；本机默认路径用于当前开发环境。
set(MYCHAT_DEFAULT_VCPKG_ROOT "/home/myself/pkgs/vcpkg")
if(NOT DEFINED VCPKG_ROOT)
    if(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "vcpkg root")
    elseif(EXISTS "${MYCHAT_DEFAULT_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(VCPKG_ROOT "${MYCHAT_DEFAULT_VCPKG_ROOT}" CACHE PATH "vcpkg root")
    endif()
endif()

if(DEFINED VCPKG_ROOT AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# 项目信息。CMAKE_TOOLCHAIN_FILE 必须在 project() 前设置。
project(MyChat VERSION 1.0.0 LANGUAGES CXX)

option(MYCHAT_BUILD_TESTS "Build MyChat tests" ON)
option(MYCHAT_BUILD_GATEWAY "Build gateway targets that require Redis/JWT/httplib dependencies" ON)
option(MYCHAT_BUILD_SERVICES "Build service targets" OFF)
option(MYCHAT_BUILD_LEGACY_GATEWAY_TESTS "Build older gateway integration tests that still need cleanup" OFF)
option(MYCHAT_BUILD_PGSQL_ODB "Enable im_pgsql library target (requires ODB runtime libraries; pass -DVCPKG_MANIFEST_FEATURES=pgsql-odb)" OFF)

# C++标准设置
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED VCPKG_ROOT)
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_ROOT}/installed/x64-linux")
    file(GLOB MYCHAT_VCPKG_PROTOC_CANDIDATES
        "${VCPKG_ROOT}/installed/x64-linux/tools/protobuf/protoc*")
    list(SORT MYCHAT_VCPKG_PROTOC_CANDIDATES)
    list(LENGTH MYCHAT_VCPKG_PROTOC_CANDIDATES MYCHAT_VCPKG_PROTOC_COUNT)
    if(MYCHAT_VCPKG_PROTOC_COUNT GREATER 0)
        list(GET MYCHAT_VCPKG_PROTOC_CANDIDATES 0 MYCHAT_VCPKG_PROTOC)
        if(EXISTS "${MYCHAT_VCPKG_PROTOC}")
            set(Protobuf_PROTOC_EXECUTABLE "${MYCHAT_VCPKG_PROTOC}" CACHE FILEPATH "Protobuf compiler")
        endif()
    endif()
endif()

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# 编译选项
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 查找依赖包（全局查找，所有子目录继承）
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(utf8_range CONFIG REQUIRED)
find_package(hiredis CONFIG QUIET)
find_package(jwt-cpp CONFIG QUIET)
find_package(httplib CONFIG QUIET)
find_package(unofficial-libuuid CONFIG QUIET)

# ODB runtime libraries (used when MYCHAT_BUILD_PGSQL_ODB is ON).
# ODB runtime packages are behind a vcpkg feature to keep the default
# dependency set free of PostgreSQL/ODB. Pass
#   -DVCPKG_MANIFEST_FEATURES=pgsql-odb
# to install them.
if(MYCHAT_BUILD_PGSQL_ODB)
    find_package(odb CONFIG COMPONENTS libodb pgsql QUIET)
    if(odb_FOUND)
        if(NOT TARGET odb::libodb)
            message(WARNING "MYCHAT_BUILD_PGSQL_ODB=ON but odb::libodb target not found. "
                "Install libodb via vcpkg (pass -DVCPKG_MANIFEST_FEATURES=pgsql-odb).")
            set(MYCHAT_HAS_ODB_RUNTIME FALSE)
        elseif(NOT TARGET odb::libodb-pgsql)
            message(WARNING "MYCHAT_BUILD_PGSQL_ODB=ON but odb::libodb-pgsql target not found. "
                "Install libodb-pgsql via vcpkg (pass -DVCPKG_MANIFEST_FEATURES=pgsql-odb).")
            set(MYCHAT_HAS_ODB_RUNTIME FALSE)
        else()
            set(MYCHAT_HAS_ODB_RUNTIME TRUE)
            message(STATUS "ODB runtime libraries found. PostgreSQL/ODB support enabled.")
        endif()
    else()
        message(WARNING "MYCHAT_BUILD_PGSQL_ODB=ON but ODB CMake package not found. "
            "Install ODB runtime libraries via vcpkg: add -DVCPKG_MANIFEST_FEATURES=pgsql-odb "
            "to your CMake configure command.")
        set(MYCHAT_HAS_ODB_RUNTIME FALSE)
    endif()
else()
    set(MYCHAT_HAS_ODB_RUNTIME FALSE)
endif()

file(GLOB MYCHAT_COMMON_PROTO_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/common/proto/*.proto")
add_custom_target(generate_proto
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            -I "${CMAKE_CURRENT_SOURCE_DIR}/common/proto"
            --cpp_out="${CMAKE_CURRENT_SOURCE_DIR}/common/proto"
            ${MYCHAT_COMMON_PROTO_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating C++ protobuf files for common/proto")

set(MYCHAT_HAS_GATEWAY_DEPS OFF)
if(hiredis_FOUND AND jwt-cpp_FOUND AND httplib_FOUND AND unofficial-libuuid_FOUND)
    set(MYCHAT_HAS_GATEWAY_DEPS ON)
endif()

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

if(MYCHAT_BUILD_GATEWAY AND MYCHAT_HAS_GATEWAY_DEPS)
    add_subdirectory(gateway)
elseif(MYCHAT_BUILD_GATEWAY)
    message(WARNING
        "Gateway targets are disabled because hiredis, jwt-cpp, httplib, or libuuid CMake packages "
        "were not found. Install dependencies with vcpkg manifest mode or configure with "
        "-DMYCHAT_BUILD_GATEWAY=OFF while working on common libraries.")
endif()

if(MYCHAT_BUILD_SERVICES)
    add_subdirectory(services)
endif()

# 启用测试
if(MYCHAT_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_subdirectory(test)
endif()
