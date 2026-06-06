# test/gateway_group_message/CMakeLists.txt
# Gateway-to-Group-Message HTTP integration tests.

add_executable(test_gateway_group_message_http
    test_gateway_group_message_http.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_gateway_group_message_http
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core
        im::gateway_auth
        im::group_service
        im::database
        im::utils
        httplib::httplib
        Threads::Threads
)

target_compile_features(test_gateway_group_message_http PRIVATE cxx_std_20)
target_compile_definitions(test_gateway_group_message_http
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME GatewayGroupMessageHttpTest COMMAND test_gateway_group_message_http)
