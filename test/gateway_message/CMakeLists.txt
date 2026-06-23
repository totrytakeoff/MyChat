# test/gateway_message/CMakeLists.txt
# Gateway-to-Message HTTP tests should target the unified command path.

# WebSocket Message handler tests
add_executable(test_gateway_message_ws
    test_gateway_message_ws.cpp
    ../support/postgres_schema.cpp
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
    ../support/postgres_schema.cpp
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

if(TARGET im::message_grpc_service)
    add_executable(test_remote_message_adapter
        test_remote_message_adapter.cpp
    )

    target_link_libraries(test_remote_message_adapter
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::message_grpc_service
            Threads::Threads
    )

    target_compile_features(test_remote_message_adapter PRIVATE cxx_std_20)

    add_test(NAME RemoteMessageServiceAdapterTest COMMAND test_remote_message_adapter)

    add_executable(test_remote_message_gateway_server_smoke
        test_remote_message_gateway_server_smoke.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_remote_message_gateway_server_smoke
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::gateway_auth
            im::message_grpc_service
            im::database
            im::utils
            httplib::httplib
            Threads::Threads
    )

    target_compile_features(test_remote_message_gateway_server_smoke PRIVATE cxx_std_20)
    target_compile_definitions(test_remote_message_gateway_server_smoke
        PRIVATE
            MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    add_test(NAME RemoteMessageGatewayServerSmokeTest
             COMMAND test_remote_message_gateway_server_smoke)
endif()
