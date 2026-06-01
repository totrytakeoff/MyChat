# test/odb/CMakeLists.txt

add_executable(test_odb_user_persistence
    test_odb_user_persistence.cpp
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
