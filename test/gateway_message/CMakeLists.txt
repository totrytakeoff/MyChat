# test/gateway_message/CMakeLists.txt
# Gateway-to-Message Service HTTP integration tests.

add_executable(test_gateway_message_http
    test_gateway_message_http.cpp
)

target_link_libraries(test_gateway_message_http
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core
        im::gateway_auth
        im::message_service
        im::database
        im::utils
        httplib::httplib
        Threads::Threads
)

target_compile_features(test_gateway_message_http PRIVATE cxx_std_20)
target_compile_definitions(test_gateway_message_http
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME GatewayMessageHttpTest COMMAND test_gateway_message_http)

# WebSocket Message handler tests
add_executable(test_gateway_message_ws
    test_gateway_message_ws.cpp
)

target_link_libraries(test_gateway_message_ws
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core
        im::gateway_auth
        im::message_service
        im::database
        im::utils
        httplib::httplib
        Threads::Threads
)

target_compile_features(test_gateway_message_ws PRIVATE cxx_std_20)
target_compile_definitions(test_gateway_message_ws
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME GatewayMessageWsTest COMMAND test_gateway_message_ws)


# PushService tests
add_executable(test_push_service
    test_push_service.cpp
)

target_link_libraries(test_push_service
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core
        im::gateway_auth
        im::message_service
        im::database
        im::utils
        httplib::httplib
        Threads::Threads
)

target_compile_features(test_push_service PRIVATE cxx_std_20)
target_compile_definitions(test_push_service
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME PushServiceTest COMMAND test_push_service)
