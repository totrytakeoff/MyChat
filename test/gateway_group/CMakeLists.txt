# test/gateway_group/CMakeLists.txt
# Gateway-to-Group Service HTTP integration tests.

add_executable(test_gateway_group_http
    test_gateway_group_http.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_gateway_group_http
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

target_compile_features(test_gateway_group_http PRIVATE cxx_std_20)
target_compile_definitions(test_gateway_group_http
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME GatewayGroupHttpTest COMMAND test_gateway_group_http)

if(TARGET im::group_grpc_service)
    add_executable(test_remote_group_client
        test_remote_group_client.cpp
    )

    target_link_libraries(test_remote_group_client
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::group_grpc_service
            Threads::Threads
    )

    target_compile_features(test_remote_group_client PRIVATE cxx_std_20)

    add_test(NAME RemoteGroupClientTest COMMAND test_remote_group_client)

    add_executable(test_remote_group_gateway_server_smoke
        test_remote_group_gateway_server_smoke.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_remote_group_gateway_server_smoke
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::gateway_auth
            im::group_grpc_service
            im::group_service
            im::user_service
            im::database
            im::utils
            httplib::httplib
            Threads::Threads
    )

    target_compile_features(test_remote_group_gateway_server_smoke PRIVATE cxx_std_20)
    target_compile_definitions(test_remote_group_gateway_server_smoke
        PRIVATE
            MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME RemoteGroupGatewayServerSmokeTest
        COMMAND test_remote_group_gateway_server_smoke
    )
endif()
