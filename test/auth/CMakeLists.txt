# test/auth/CMakeLists.txt

add_executable(test_auth_tokens
    test_auth_tokens.cpp
)

target_link_libraries(test_auth_tokens
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_auth
        im::database
        im::utils
        Threads::Threads
)

target_compile_features(test_auth_tokens PRIVATE cxx_std_20)
target_compile_definitions(test_auth_tokens
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME AuthTokenTest COMMAND test_auth_tokens)
