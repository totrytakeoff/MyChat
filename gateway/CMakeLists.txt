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
        ${CMAKE_CURRENT_SOURCE_DIR}/http
        ${CMAKE_CURRENT_SOURCE_DIR}/push
        ${CMAKE_CURRENT_SOURCE_DIR}/ws
)

# 创建命名空间别名
add_library(im::gateway_core ALIAS im_gateway_core)

# User HTTP Controller — 仅当 User Service 可用时编译和链接
if(TARGET im::user_service)
    target_sources(im_gateway_core PRIVATE http/user_http_controller.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::user_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_USER_HTTP)
    message(STATUS "User HTTP controller enabled (im::user_service available)")
else()
    message(STATUS "User HTTP controller disabled (im::user_service not available)")
endif()

# Message HTTP Controller — 仅当 Message Service 可用时编译和链接
if(TARGET im::message_service)
    target_sources(im_gateway_core PRIVATE http/message_http_controller.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::message_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_MESSAGE_HTTP)
    message(STATUS "Message HTTP controller enabled (im::message_service available)")
else()
    message(STATUS "Message HTTP controller disabled (im::message_service not available)")
endif()

# PushService — only when Message Service is available
if(TARGET im::message_service)
    target_sources(im_gateway_core PRIVATE push/push_service.cpp)
    message(STATUS "PushService enabled (im::message_service available)")
else()
    message(STATUS "PushService disabled (im::message_service not available)")
endif()

if(TARGET im::message_service)
    target_sources(im_gateway_core PRIVATE push/fanout_policies.cpp)
    message(STATUS "Fanout policies enabled (im::message_service available)")
endif()

# Message WS Handler — 仅当 Message Service 可用时编译和链接
if(TARGET im::message_service)
    target_sources(im_gateway_core PRIVATE ws/message_ws_handler.cpp)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_MESSAGE_HTTP)
    message(STATUS "Message WS handler enabled (im::message_service available)")
else()
    message(STATUS "Message WS handler disabled (im::message_service not available)")
endif()
# Friend HTTP Controller — 仅当 Friend Service 可用时编译和链接
if(TARGET im::friend_service)
    target_sources(im_gateway_core PRIVATE http/friend_http_controller.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::friend_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_FRIEND_HTTP)
    message(STATUS "Friend HTTP controller enabled (im::friend_service available)")
else()
    message(STATUS "Friend HTTP controller disabled (im::friend_service not available)")
endif()

# Group HTTP Controller — 仅当 Group Service 可用时编译和链接
if(TARGET im::group_service)
    target_sources(im_gateway_core PRIVATE http/group_http_controller.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::group_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_GROUP_HTTP)
    message(STATUS "Group HTTP controller enabled (im::group_service available)")
else()
    message(STATUS "Group HTTP controller disabled (im::group_service not available)")
endif()

# Group Message HTTP Controller — 仅当 Group Service 和 PushService 可用时编译和链接
if(TARGET im::group_service AND TARGET im::message_service)
    target_sources(im_gateway_core PRIVATE http/group_message_http_controller.cpp)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_GROUP_MESSAGE_HTTP)
    message(STATUS "Group Message HTTP controller enabled (im::group_service + im::message_service available)")
else()
    message(STATUS "Group Message HTTP controller disabled")
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
if(MYCHAT_BUILD_LEGACY_GATEWAY_TESTS)
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
endif()
