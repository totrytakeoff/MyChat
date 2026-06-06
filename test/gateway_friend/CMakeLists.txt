# test/gateway_friend/CMakeLists.txt
# Gateway-to-Friend Service HTTP integration tests.

add_executable(test_gateway_friend_http
    test_gateway_friend_http.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_gateway_friend_http
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core
        im::gateway_auth
        im::friend_service
        im::database
        im::utils
        httplib::httplib
        Threads::Threads
)

target_compile_features(test_gateway_friend_http PRIVATE cxx_std_20)
target_compile_definitions(test_gateway_friend_http
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME GatewayFriendHttpTest COMMAND test_gateway_friend_http)
