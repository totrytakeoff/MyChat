# test/gateway_user/CMakeLists.txt
# Gateway-to-User Service HTTP integration tests.

add_executable(test_gateway_user_http
    test_gateway_user_http.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_gateway_user_http
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core
        im::gateway_auth
        im::user_service
        im::database
        im::utils
        httplib::httplib
        Threads::Threads
)

target_compile_features(test_gateway_user_http PRIVATE cxx_std_20)
target_compile_definitions(test_gateway_user_http
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME GatewayUserHttpTest COMMAND test_gateway_user_http)

if(TARGET im::user_grpc_service)
    add_executable(test_remote_user_client
        test_remote_user_client.cpp
    )

    target_link_libraries(test_remote_user_client
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::user_grpc_service
            Threads::Threads
    )

    target_compile_features(test_remote_user_client PRIVATE cxx_std_20)

    add_test(NAME RemoteUserClientTest COMMAND test_remote_user_client)

    add_executable(test_remote_user_gateway_server_smoke
        test_remote_user_gateway_server_smoke.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_remote_user_gateway_server_smoke
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::gateway_auth
            im::user_grpc_service
            im::database
            im::utils
            httplib::httplib
            Threads::Threads
    )

    target_compile_features(test_remote_user_gateway_server_smoke PRIVATE cxx_std_20)
    target_compile_definitions(test_remote_user_gateway_server_smoke
        PRIVATE
            MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    add_test(NAME RemoteUserGatewayServerSmokeTest
             COMMAND test_remote_user_gateway_server_smoke)
endif()
