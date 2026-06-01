# test/db/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(RedisManagerTests)

# 继承根目录的设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(FALSE)
    # TODO(Phase 1): Rewrite this test for the hiredis-backed RedisClient API.
    # The existing test_redis_mgr.cpp still depends on redis-plus-plus types and
    # high-level commands that were intentionally removed from the MVP wrapper.
    add_executable(test_redis
        test_redis_mgr.cpp
    )

    target_link_libraries(test_redis
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::database
            im::utils
            nlohmann_json::nlohmann_json
            spdlog::spdlog
            Threads::Threads
    )

    target_compile_features(test_redis PRIVATE cxx_std_17)
    add_test(NAME RedisManagerTests COMMAND test_redis)
endif()

add_executable(test_redis_hiredis
    test_redis_hiredis.cpp
)

target_link_libraries(test_redis_hiredis
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::database
        im::utils
        Threads::Threads
)

target_compile_features(test_redis_hiredis PRIVATE cxx_std_20)
add_test(NAME RedisHiredisTest COMMAND test_redis_hiredis)
