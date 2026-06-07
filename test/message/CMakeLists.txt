# test/message/CMakeLists.txt

add_executable(test_message_service
    test_message_service.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_message_service
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::message_service
        Threads::Threads
)

target_compile_features(test_message_service PRIVATE cxx_std_20)

add_test(NAME MessageServiceCoreTest COMMAND test_message_service)

if(TARGET im::message_grpc_service)
    add_executable(test_message_grpc_service
        test_message_grpc_service.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_message_grpc_service
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::message_grpc_service
            Threads::Threads
    )

    target_compile_features(test_message_grpc_service PRIVATE cxx_std_20)

    add_test(NAME MessageGrpcServiceTest COMMAND test_message_grpc_service)

    add_executable(test_message_server_app
        test_message_server_app.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_message_server_app
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::message_grpc_service
            Threads::Threads
    )

    target_compile_features(test_message_server_app PRIVATE cxx_std_20)

    add_test(NAME MessageServerAppTest COMMAND test_message_server_app)
endif()
