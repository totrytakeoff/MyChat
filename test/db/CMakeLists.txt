cmake_minimum_required(VERSION 3.16)

# 设置vcpkg工具链文件
set(CMAKE_TOOLCHAIN_FILE "/home/myself/vcpkg/scripts/buildsystems/vcpkg.cmake" 
    CACHE STRING "Vcpkg toolchain file")

# 项目声明
project(RedisManagerTests)

# C++标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(GTest REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/database
)

# 设置源文件路径
set(REDIS_MGR_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../common/database/redis_mgr.cpp")
set(LOG_MGR_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../common/utils/log_manager.cpp")

# 检查源文件是否存在
if(NOT EXISTS ${REDIS_MGR_SOURCE})
    message(FATAL_ERROR "Redis manager source not found: ${REDIS_MGR_SOURCE}")
endif()

if(NOT EXISTS ${LOG_MGR_SOURCE})
    message(FATAL_ERROR "Log manager source not found: ${LOG_MGR_SOURCE}")
endif()

# 创建测试可执行文件
add_executable(test_redis
    test_redis_mgr.cpp
    ${REDIS_MGR_SOURCE}
    ${LOG_MGR_SOURCE}
)

# 设置编译选项
target_compile_features(test_redis PRIVATE cxx_std_17)

# 链接库
target_link_libraries(test_redis
    PRIVATE
    GTest::GTest
    GTest::Main
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    redis++::redis++_static
    pthread
)

# 添加测试
enable_testing()
add_test(NAME RedisManagerTests COMMAND test_redis)
