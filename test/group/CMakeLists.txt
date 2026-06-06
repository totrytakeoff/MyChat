# test/group/CMakeLists.txt
# Group Service core unit tests.

add_executable(test_group_service
    test_group_service.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_group_service
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::group_service
        Threads::Threads
)

target_compile_features(test_group_service PRIVATE cxx_std_20)

add_test(NAME GroupServiceCoreTest COMMAND test_group_service)
