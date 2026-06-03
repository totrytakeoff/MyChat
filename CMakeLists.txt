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
option(MYCHAT_BUILD_USER_SERVICE "Build User Service target (requires ODB)" ON)
option(MYCHAT_BUILD_MESSAGE_SERVICE "Build Message Service target (requires ODB)" ON)
option(MYCHAT_BUILD_FRIEND_SERVICE "Build Friend Service target (requires ODB and User Service)" ON)
option(MYCHAT_BUILD_GROUP_SERVICE "Build Group Service target (requires ODB and User Service)" ON)
option(MYCHAT_BUILD_CODEC_SERVICE "Build legacy codec gRPC service target (requires gRPC)" OFF)
option(MYCHAT_BUILD_LEGACY_GATEWAY_TESTS "Build older gateway integration tests that still need cleanup" OFF)
option(MYCHAT_BUILD_LEGACY_UNIT_TESTS "Build older unit tests pending cleanup" OFF)
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

# ODB 2.5.0 runtime detection.
#
# The generated ODB files (services/odb/generated/user-odb.hxx) require
# ODB runtime version 2.5.0 (ODB_VERSION==20500). CMake finds the runtime
# through these paths, in priority order:
#
#   1. MYCHAT_ODB_ROOT variable (explicit path, e.g. -DMYCHAT_ODB_ROOT=/opt/odb)
#   2. .odb/installed/ (default, created by scripts/build_odb_runtime_2_5.sh)
#
# If a compatible runtime is not found, CMake fails at configure time with
# instructions to run the build script. There is no silent fallback to
# vcpkg's ODB 2.4.0 — that version is incompatible with the generated files.
if(MYCHAT_BUILD_PGSQL_ODB)
    set(_odb_prefix "")
    set(_odb_err_suffix "")

    # Priority 1: MYCHAT_ODB_ROOT
    if(NOT "${MYCHAT_ODB_ROOT}" STREQUAL "")
        if(EXISTS "${MYCHAT_ODB_ROOT}/lib/libodb.a")
            set(_odb_prefix "${MYCHAT_ODB_ROOT}")
            message(STATUS "MYCHAT_ODB_ROOT=${_odb_prefix}")
        else()
            message(FATAL_ERROR
                "MYCHAT_ODB_ROOT=${MYCHAT_ODB_ROOT} does not contain libodb.a. "
                "Set MYCHAT_ODB_ROOT to the ODB 2.5.0 install prefix.")
        endif()
    endif()

    # Priority 2: default .odb/installed location
    if("${_odb_prefix}" STREQUAL "")
        set(_odb_default "${CMAKE_SOURCE_DIR}/.odb/installed")
        if(EXISTS "${_odb_default}/lib/libodb.a")
            set(_odb_prefix "${_odb_default}")
            message(STATUS "Using ODB runtime from default location: ${_odb_default}")
        endif()
    endif()

    # Verify version and define imported targets
    if(NOT "${_odb_prefix}" STREQUAL "")
        set(_vh "${_odb_prefix}/include/odb/version.hxx")
        if(NOT EXISTS "${_vh}")
            message(FATAL_ERROR
                "ODB runtime at ${_odb_prefix} is missing version.hxx. "
                "Reinstall the ODB 2.5.0 runtime.")
        endif()

        file(READ "${_vh}" _version_hxx)
        if(_version_hxx MATCHES "#define[ \t]+ODB_VERSION[ \t]+20500")
            message(STATUS "Found ODB runtime 2.5.0 at ${_odb_prefix}")

            # Add include path early to shadow any vcpkg 2.4.0 headers.
            include_directories(BEFORE "${_odb_prefix}/include")

            add_library(odb::libodb STATIC IMPORTED GLOBAL)
            set_target_properties(odb::libodb PROPERTIES
                IMPORTED_LOCATION "${_odb_prefix}/lib/libodb.a"
                INTERFACE_INCLUDE_DIRECTORIES "${_odb_prefix}/include"
            )

            find_package(PostgreSQL QUIET)

            add_library(odb::libodb-pgsql STATIC IMPORTED GLOBAL)
            set_property(TARGET odb::libodb-pgsql PROPERTY IMPORTED_LOCATION
                "${_odb_prefix}/lib/libodb-pgsql.a")
            set_property(TARGET odb::libodb-pgsql APPEND PROPERTY
                INTERFACE_INCLUDE_DIRECTORIES "${_odb_prefix}/include")
            set_property(TARGET odb::libodb-pgsql APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES odb::libodb)
            if(PostgreSQL_FOUND)
                set_property(TARGET odb::libodb-pgsql APPEND PROPERTY
                    INTERFACE_INCLUDE_DIRECTORIES "${PostgreSQL_INCLUDE_DIRS}")
                set_property(TARGET odb::libodb-pgsql APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "PostgreSQL::PostgreSQL")
            endif()

            set(MYCHAT_HAS_ODB_RUNTIME TRUE)
            message(STATUS "PostgreSQL/ODB support enabled.")
        else()
            string(REGEX MATCH "#define[ \t]+ODB_VERSION[ \t]+([0-9]+)" _vm "${_version_hxx}")
            message(FATAL_ERROR
                "ODB runtime at ${_odb_prefix} has ODB_VERSION=${CMAKE_MATCH_1}, "
                "but version 20500 (2.5.0) is required.\n"
                "Rebuild the runtime:\n"
                "  scripts/build_odb_runtime_2_5.sh${_odb_err_suffix}")
        endif()
    else()
        # No ODB runtime found at any location.
        set(MYCHAT_HAS_ODB_RUNTIME FALSE)

        # Check if generated ODB files exist — they mandate 2.5.0.
        if(EXISTS "${CMAKE_SOURCE_DIR}/services/odb/generated/user-odb.hxx")
            message(FATAL_ERROR
                "ODB 2.5.0 runtime not found.\n"
                "Generated ODB files require runtime 2.5.0.\n\n"
                "To build the ODB 2.5.0 runtime, run:\n"
                "  scripts/build_odb_runtime_2_5.sh\n\n"
                "Or set MYCHAT_ODB_ROOT to an existing installation:\n"
                "  cmake ... -DMYCHAT_ODB_ROOT=/path/to/odb-2.5.0 ...\n\n"
                "Also ensure vcpkg feature pgsql-odb is enabled:\n"
                "  -DVCPKG_MANIFEST_FEATURES=pgsql-odb")
        else()
            message(WARNING
                "ODB 2.5.0 runtime not found.\n"
                "Run the build script to enable ODB support:\n"
                "  scripts/build_odb_runtime_2_5.sh")
        endif()
    endif()

    unset(_odb_prefix)
    unset(_odb_err_suffix)
    unset(_odb_default)
    unset(_vh)
    unset(_version_hxx)
    unset(_vm)
else()
    set(MYCHAT_HAS_ODB_RUNTIME FALSE)
endif()

# ODB-generated user persistence model (requires ODB runtime).
if(MYCHAT_BUILD_PGSQL_ODB AND MYCHAT_HAS_ODB_RUNTIME)
    add_subdirectory(services/odb)
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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/services/odb)

# 添加子目录
add_subdirectory(common)

if(MYCHAT_BUILD_SERVICES)
    add_subdirectory(services)
endif()

if(MYCHAT_BUILD_GATEWAY AND MYCHAT_HAS_GATEWAY_DEPS)
    add_subdirectory(gateway)
elseif(MYCHAT_BUILD_GATEWAY)
    message(WARNING
        "Gateway targets are disabled because hiredis, jwt-cpp, httplib, or libuuid CMake packages "
        "were not found. Install dependencies with vcpkg manifest mode or configure with "
        "-DMYCHAT_BUILD_GATEWAY=OFF while working on common libraries.")
endif()

# 启用测试
if(MYCHAT_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_subdirectory(test)
endif()
