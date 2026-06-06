# test/user/CMakeLists.txt

add_executable(test_user_service
    test_user_service.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_user_service
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::user_service
        Threads::Threads
)

target_compile_features(test_user_service PRIVATE cxx_std_20)

add_test(NAME UserServiceCoreTest COMMAND test_user_service)

if(TARGET im::user_grpc_service)
    add_executable(test_user_grpc_service
        test_user_grpc_service.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_user_grpc_service
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::user_grpc_service
            Threads::Threads
    )

    target_compile_features(test_user_grpc_service PRIVATE cxx_std_20)

    add_test(NAME UserGrpcServiceTest COMMAND test_user_grpc_service)
endif()
