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

add_executable(test_friend_packet_dispatcher
    test_friend_packet_dispatcher.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_friend_packet_dispatcher
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::friend_service
        Threads::Threads
)

target_compile_features(test_friend_packet_dispatcher PRIVATE cxx_std_20)

add_test(NAME FriendPacketDispatcherTest COMMAND test_friend_packet_dispatcher)

if(TARGET im::friend_grpc_service)
    add_executable(test_friend_grpc_service
        test_friend_grpc_service.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_friend_grpc_service
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::friend_grpc_service
            Threads::Threads
    )

    target_compile_features(test_friend_grpc_service PRIVATE cxx_std_20)

    add_test(NAME FriendGrpcServiceTest COMMAND test_friend_grpc_service)

    add_executable(test_friend_server_app
        test_friend_server_app.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_friend_server_app
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::friend_grpc_service
            Threads::Threads
    )

    target_compile_features(test_friend_server_app PRIVATE cxx_std_20)

    add_test(NAME FriendServerAppTest COMMAND test_friend_server_app)
endif()
