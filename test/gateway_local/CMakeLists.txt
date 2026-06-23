# test/gateway_local/CMakeLists.txt
# Local all-in-one Gateway smoke tests cover the unified HTTP entry with
# all service modes set to local.

add_executable(test_local_gateway_http_smoke
    test_local_gateway_http_smoke.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_local_gateway_http_smoke
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::gateway_core
        im::gateway_auth
        im::database
        im::utils
        httplib::httplib
        Threads::Threads
)

target_compile_features(test_local_gateway_http_smoke PRIVATE cxx_std_20)
target_compile_definitions(test_local_gateway_http_smoke
    PRIVATE
        MYCHAT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME LocalGatewayHttpSmokeTest COMMAND test_local_gateway_http_smoke)
