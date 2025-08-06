cmake_minimum_required(VERSION 3.16)
project(MyChat-Gateway-Demo)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 查找必要的包
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(PkgConfig REQUIRED)

# 可选的OpenSSL支持（用于认证）
find_package(OpenSSL)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found - enabling crypto support")
    add_definitions(-DWITH_OPENSSL)
else()
    message(WARNING "OpenSSL not found - using simplified crypto")
endif()

# 设置包含目录
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/../..)  # 访问项目根目录
include_directories(${CMAKE_SOURCE_DIR}/../../common)
include_directories(${Protobuf_INCLUDE_DIRS})

# httplib 支持
# 检查是否已下载httplib
set(HTTPLIB_DIR "${CMAKE_SOURCE_DIR}/third_party/httplib")
if(NOT EXISTS "${HTTPLIB_DIR}/httplib.h")
    message(STATUS "httplib.h not found, attempting to download...")
    file(MAKE_DIRECTORY "${HTTPLIB_DIR}")
    
    # 尝试下载httplib.h
    if(NOT EXISTS "${HTTPLIB_DIR}/httplib.h")
        message(WARNING "httplib.h not found at ${HTTPLIB_DIR}")
        message(WARNING "Please download manually:")
        message(WARNING "  mkdir -p ${HTTPLIB_DIR}")
        message(WARNING "  wget -O ${HTTPLIB_DIR}/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h")
        message(FATAL_ERROR "httplib.h is required for HTTP support")
    endif()
else()
    message(STATUS "Found httplib.h at ${HTTPLIB_DIR}")
endif()

include_directories(${CMAKE_SOURCE_DIR}/third_party/httplib)

# 收集源文件
file(GLOB_RECURSE DEMO_SOURCES
    "src/*.cpp"
    "src/*.hpp"
)

# 收集公共源文件
file(GLOB_RECURSE COMMON_NETWORK_SOURCES
    "../../common/network/*.cpp"
)

file(GLOB_RECURSE COMMON_UTILS_SOURCES  
    "../../common/utils/*.cpp"
)

file(GLOB_RECURSE COMMON_PROTO_SOURCES
    "../../common/proto/*.cc"
)

# 合并所有源文件
set(ALL_SOURCES
    ${DEMO_SOURCES}
    ${COMMON_NETWORK_SOURCES}
    ${COMMON_UTILS_SOURCES}
    ${COMMON_PROTO_SOURCES}
)

# 移除不需要的测试文件
list(FILTER ALL_SOURCES EXCLUDE REGEX ".*test.*")
list(FILTER ALL_SOURCES EXCLUDE REGEX ".*Test.*")

# 创建可执行文件
add_executable(gateway_server_demo ${ALL_SOURCES})

# 设置目标属性
set_target_properties(gateway_server_demo PROPERTIES
    OUTPUT_NAME "gateway_server"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 链接库
target_link_libraries(gateway_server_demo
    ${Protobuf_LIBRARIES}
    ${Boost_LIBRARIES}
    pthread
)

# 如果找到OpenSSL，链接OpenSSL库
if(OpenSSL_FOUND)
    target_link_libraries(gateway_server_demo OpenSSL::SSL OpenSSL::Crypto)
endif()

# 复制配置文件到输出目录
configure_file(
    ${CMAKE_SOURCE_DIR}/config/gateway_config.json
    ${CMAKE_BINARY_DIR}/bin/config/gateway_config.json
    COPYONLY
)

# 创建必要的目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/logs)

# 创建启动脚本
configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/start_gateway.sh.in
    ${CMAKE_BINARY_DIR}/bin/start_gateway.sh
    @ONLY
)

# 安装规则
install(TARGETS gateway_server_demo 
    DESTINATION bin
)

install(FILES config/gateway_config.json 
    DESTINATION bin/config
)

install(DIRECTORY DESTINATION bin/logs)

# 开发工具目标
add_custom_target(clean-logs
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/bin/logs/*
    COMMENT "Cleaning log files"
)

add_custom_target(run
    COMMAND ${CMAKE_BINARY_DIR}/bin/gateway_server ${CMAKE_BINARY_DIR}/bin/config/gateway_config.json
    DEPENDS gateway_server_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running gateway server"
)

add_custom_target(debug
    COMMAND gdb --args ${CMAKE_BINARY_DIR}/bin/gateway_server ${CMAKE_BINARY_DIR}/bin/config/gateway_config.json
    DEPENDS gateway_server_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running gateway server in debugger"
)

# 打印构建信息
message(STATUS "=== MyChat Gateway Demo Build Configuration ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Protobuf Version: ${Protobuf_VERSION}")
message(STATUS "Boost Version: ${Boost_VERSION}")
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
endif()
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Output Directory: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "=============================================")

# 构建后提示信息
add_custom_command(TARGET gateway_server_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "🎉 Gateway Demo Build Complete!"
    COMMAND ${CMAKE_COMMAND} -E echo "📁 Output: ${CMAKE_BINARY_DIR}/bin/"
    COMMAND ${CMAKE_COMMAND} -E echo "🚀 Run: cd ${CMAKE_BINARY_DIR}/bin && ./gateway_server"
    COMMAND ${CMAKE_COMMAND} -E echo "🏃 Quick: make run"
    COMMAND ${CMAKE_COMMAND} -E echo "🐛 Debug: make debug"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)