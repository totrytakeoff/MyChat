cmake_minimum_required(VERSION 3.16)
project(MinimalGateway)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找vcpkg包
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

# 创建可执行文件
add_executable(minimal_gateway_server
    minimal_gateway.cpp
)

# 链接库
target_link_libraries(minimal_gateway_server
    nlohmann_json::nlohmann_json
    httplib::httplib
    pthread
)