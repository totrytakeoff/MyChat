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
endif()

if(TARGET im::push_grpc_service AND TARGET im::gateway_core)
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
