# test/db/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(RedisManagerTests)

# 继承根目录的设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 创建测试可执行文件
add_executable(test_redis
    test_redis_mgr.cpp
)

# 链接库（使用新的组件库）
target_link_libraries(test_redis
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::database  # 使用新的数据库库
        im::utils     # 使用新的工具库
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        redis++::redis++_static
        Threads::Threads
)

# 编译选项
target_compile_features(test_redis PRIVATE cxx_std_17)

# 添加测试
enable_testing()
add_test(NAME RedisManagerTests COMMAND test_redis)