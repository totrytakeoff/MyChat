# test/friend/CMakeLists.txt
# Friend Service core unit tests.

add_executable(test_friend_service
    test_friend_service.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_friend_service
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::friend_service
        Threads::Threads
)

target_compile_features(test_friend_service PRIVATE cxx_std_20)

add_test(NAME FriendServiceCoreTest COMMAND test_friend_service)
