# test/CMakeLists.txt

# 确保测试可以找到所有组件
include_directories(
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/gateway
    ${CMAKE_SOURCE_DIR}/services
)

# 添加所有测试子目录
if(TARGET im::database)
    add_subdirectory(db)
else()
    message(STATUS "Skipping database tests because im::database is not built.")
endif()
if(TARGET im::user_odb)
    add_subdirectory(odb)
endif()
if(TARGET im::user_service)
    add_subdirectory(user)
endif()
if(TARGET im::gateway_auth)
    add_subdirectory(auth)
endif()
add_subdirectory(network)
if(MYCHAT_BUILD_LEGACY_GATEWAY_TESTS AND TARGET im::gateway_core)
    add_subdirectory(gateway)
elseif(TARGET im::gateway_core)
    message(STATUS "Skipping legacy gateway integration tests. Enable MYCHAT_BUILD_LEGACY_GATEWAY_TESTS after cleanup.")
endif()
if(TARGET im::gateway_core OR EXISTS "${CMAKE_SOURCE_DIR}/gateway/router/router.mgr.cpp")
    add_subdirectory(router)
endif()
add_subdirectory(utils)
