# test/odb/CMakeLists.txt

add_executable(test_odb_user_persistence
    test_odb_user_persistence.cpp
    ../support/postgres_schema.cpp
)

target_link_libraries(test_odb_user_persistence
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        im::user_odb
        Threads::Threads
)

target_compile_features(test_odb_user_persistence PRIVATE cxx_std_20)
target_include_directories(test_odb_user_persistence
    PRIVATE
        ${CMAKE_SOURCE_DIR}/services/odb
        ${CMAKE_SOURCE_DIR}/services/odb/generated
)
add_test(NAME ODBUserPersistenceTest COMMAND test_odb_user_persistence)

if(TARGET im::pgsql)
    add_executable(test_pgsql_connection
        test_pgsql_connection.cpp
        ../support/postgres_schema.cpp
    )

    target_link_libraries(test_pgsql_connection
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            im::pgsql
            im::user_odb
            Threads::Threads
    )

    target_compile_features(test_pgsql_connection PRIVATE cxx_std_20)
    target_include_directories(test_pgsql_connection
        PRIVATE
            ${CMAKE_SOURCE_DIR}/common
            ${CMAKE_SOURCE_DIR}/services/odb
            ${CMAKE_SOURCE_DIR}/services/odb/generated
    )
    add_test(NAME PgSqlConnectionTest COMMAND test_pgsql_connection)
endif()
