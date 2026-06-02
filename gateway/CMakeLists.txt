# gateway/CMakeLists.txt

add_library(im_gateway_core STATIC
    gateway_server/gateway_server.cpp
    connection_manager/connection_manager.cpp
    router/router.mgr.cpp
    message_processor/message_parser.cpp
    message_processor/coro_message_processor.cpp
    message_processor/message_processor.cpp
)

target_link_libraries(im_gateway_core
    PUBLIC
        im::network
        im::utils
        im::database
        im::proto
        jwt-cpp::jwt-cpp
        httplib::httplib
        unofficial::UUID::uuid
        ${ABSL_LIBRARIES}
)

target_include_directories(im_gateway_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/gateway_server
        ${CMAKE_CURRENT_SOURCE_DIR}/connection_manager
        ${CMAKE_CURRENT_SOURCE_DIR}/router
        ${CMAKE_CURRENT_SOURCE_DIR}/message_processor
)

# 创建命名空间别名
add_library(im::gateway_core ALIAS im_gateway_core)

# User HTTP Controller — 仅当 User Service 可用时编译和链接
if(TARGET im::user_service)
    target_sources(im_gateway_core PRIVATE user_http_controller.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::user_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_USER_HTTP)
    message(STATUS "User HTTP controller enabled (im::user_service available)")
else()
    message(STATUS "User HTTP controller disabled (im::user_service not available)")
endif()

# Message HTTP Controller — 仅当 Message Service 可用时编译和链接
if(TARGET im::message_service)
    target_sources(im_gateway_core PRIVATE message_http_controller.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::message_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_MESSAGE_HTTP)
    message(STATUS "Message HTTP controller enabled (im::message_service available)")
else()
    message(STATUS "Message HTTP controller disabled (im::message_service not available)")
endif()

# Message WS Handler — 仅当 Message Service 可用时编译和链接
if(TARGET im::message_service)
    target_sources(im_gateway_core PRIVATE message_ws_handler.cpp)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_MESSAGE_HTTP)
    message(STATUS "Message WS handler enabled (im::message_service available)")
else()
    message(STATUS "Message WS handler disabled (im::message_service not available)")
endif()

add_library(im_gateway_auth STATIC
    auth/multi_platform_auth.cpp
)

target_link_libraries(im_gateway_auth
    PUBLIC
        im::utils
        im::database
        jwt-cpp::jwt-cpp
        nlohmann_json::nlohmann_json
        unofficial::UUID::uuid
)

target_include_directories(im_gateway_auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/auth
)

# 创建命名空间别名
add_library(im::gateway_auth ALIAS im_gateway_auth)

add_executable(gateway_server
    main.cpp
)

target_link_libraries(gateway_server
    PRIVATE
        im::gateway_core
        im::gateway_auth
        im::network
        im::utils
        im::database
        im::proto
)
add_executable(gateway_server_test
    main_example.cpp
)

target_link_libraries(gateway_server_test
    PRIVATE
        im::gateway_core
        im::gateway_auth
        im::network
        im::utils
        im::database
        im::proto
)
