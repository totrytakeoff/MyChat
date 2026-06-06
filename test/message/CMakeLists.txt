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
