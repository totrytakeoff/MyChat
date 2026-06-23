# test/gateway_friend/CMakeLists.txt
# Gateway-to-Friend HTTP tests should target the unified command path.

if(TARGET im::friend_grpc_service)
    add_executable(test_remote_friend_adapter
        test_remote_friend_adapter.cpp
    )

    target_link_libraries(test_remote_friend_adapter
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::friend_grpc_service
            Threads::Threads
    )

    target_compile_features(test_remote_friend_adapter PRIVATE cxx_std_20)

    add_test(NAME RemoteFriendServiceAdapterTest COMMAND test_remote_friend_adapter)

    add_executable(test_remote_friend_gateway_server_smoke
        test_remote_friend_gateway_server_smoke.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_remote_friend_gateway_server_smoke
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::gateway_core
            im::gateway_auth
            im::friend_grpc_service
            im::friend_service
            im::user_service
            im::database
            im::utils
            httplib::httplib
            Threads::Threads
    )

    target_compile_features(test_remote_friend_gateway_server_smoke PRIVATE cxx_std_20)
    target_compile_definitions(test_remote_friend_gateway_server_smoke
        PRIVATE
            MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME RemoteFriendGatewayServerSmokeTest
        COMMAND test_remote_friend_gateway_server_smoke
    )
endif()
