# test/push/CMakeLists.txt
# Push Service runtime unit tests.

add_executable(test_push_runtime
    test_push_runtime.cpp
)

target_link_libraries(test_push_runtime
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::push_service
        im::network
        im::proto
        Threads::Threads
)

target_compile_features(test_push_runtime PRIVATE cxx_std_20)

add_test(NAME PushRuntimeTest COMMAND test_push_runtime)

if(TARGET im::push_grpc_service)
    add_executable(test_push_grpc_service
        test_push_grpc_service.cpp
    )

    target_link_libraries(test_push_grpc_service
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::push_grpc_service
            Threads::Threads
    )

    target_compile_features(test_push_grpc_service PRIVATE cxx_std_20)

    add_test(NAME PushGrpcServiceTest COMMAND test_push_grpc_service)

    add_executable(test_push_server_app
        test_push_server_app.cpp
    )

    target_link_libraries(test_push_server_app
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::push_grpc_service
            Threads::Threads
    )

    target_compile_features(test_push_server_app PRIVATE cxx_std_20)

    add_test(NAME PushServerAppTest COMMAND test_push_server_app)

    add_executable(test_push_server_remote_adapters
        test_push_server_remote_adapters.cpp
    )

    target_link_libraries(test_push_server_remote_adapters
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::push_grpc_service
            Threads::Threads
    )

    target_compile_features(test_push_server_remote_adapters PRIVATE cxx_std_20)

    add_test(NAME PushServerRemoteAdaptersTest COMMAND test_push_server_remote_adapters)
endif()

if(TARGET im::push_grpc_service AND TARGET im::gateway_core)
    add_executable(test_gateway_push_delivery_service
        test_gateway_push_delivery_service.cpp
    )

    target_link_libraries(test_gateway_push_delivery_service
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::push_grpc_service
            Threads::Threads
    )

    target_compile_features(test_gateway_push_delivery_service PRIVATE cxx_std_20)

    add_test(NAME GatewayPushDeliveryServiceTest COMMAND test_gateway_push_delivery_service)

    add_executable(test_remote_push_e2e_smoke
        test_remote_push_e2e_smoke.cpp
    )

    target_link_libraries(test_remote_push_e2e_smoke
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::push_grpc_service
            im::network
            Threads::Threads
    )

    target_compile_features(test_remote_push_e2e_smoke PRIVATE cxx_std_20)

    add_test(NAME RemotePushEndToEndSmokeTest COMMAND test_remote_push_e2e_smoke)

    add_executable(test_remote_push_notifier
        test_remote_push_notifier.cpp
    )

    target_link_libraries(test_remote_push_notifier
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::push_grpc_service
            Threads::Threads
    )

    target_compile_features(test_remote_push_notifier PRIVATE cxx_std_20)

    add_test(NAME RemotePushNotifierTest COMMAND test_remote_push_notifier)
endif()

if(TARGET im::push_grpc_service
   AND TARGET im::gateway_core
   AND TARGET im::gateway_auth
   AND TARGET im::message_service
   AND TARGET im::group_service
   AND TARGET im::user_service)
    add_executable(test_remote_push_gateway_entrypoints
        test_remote_push_gateway_entrypoints.cpp
    )

    target_link_libraries(test_remote_push_gateway_entrypoints
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::gateway_auth
            im::message_service
            im::group_service
            im::user_service
            im::database
            im::utils
            httplib::httplib
            Threads::Threads
    )

    target_compile_features(test_remote_push_gateway_entrypoints PRIVATE cxx_std_20)
    target_compile_definitions(test_remote_push_gateway_entrypoints
        PRIVATE
            MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    add_test(NAME RemotePushGatewayEntrypointsTest
             COMMAND test_remote_push_gateway_entrypoints)

    add_executable(test_remote_push_gateway_server_smoke
        test_remote_push_gateway_server_smoke.cpp
    )

    target_link_libraries(test_remote_push_gateway_server_smoke
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::gateway_auth
            im::message_service
            im::group_service
            im::user_service
            im::push_grpc_service
            im::database
            im::network
            im::utils
            httplib::httplib
            Threads::Threads
    )

    target_compile_features(test_remote_push_gateway_server_smoke PRIVATE cxx_std_20)
    target_compile_definitions(test_remote_push_gateway_server_smoke
        PRIVATE
            MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    add_test(NAME RemotePushGatewayServerSmokeTest
             COMMAND test_remote_push_gateway_server_smoke)
endif()
