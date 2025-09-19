# test/CMakeLists.txt

# 确保测试可以找到所有组件
include_directories(
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/gateway
    ${CMAKE_SOURCE_DIR}/services
)

# 定义测试宏
macro(add_unit_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME}
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            ${ARGN}
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "unit")
endmacro()

macro(add_integration_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME}
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            ${ARGN}
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "integration")
endmacro()

# 添加所有测试子目录
add_subdirectory(db)
add_subdirectory(network)
add_subdirectory(gateway)
add_subdirectory(router)
add_subdirectory(utils)
add_subdirectory(temp)