# gateway/CMakeLists.txt

# 提高 httplib 的 listen backlog(TCP 服务端 listen(fd, backlog) 里的连接排队长度)，适用于高并发场景
set(MYCHAT_HTTP_LISTEN_BACKLOG 4096 CACHE STRING
    "cpp-httplib listen backlog used by gateway HTTP server")

add_library(im_gateway_core STATIC
    gateway_server/gateway_server.cpp
    command_handlers/gateway_command_handler_registry.cpp
    command_handlers/command_handler_utils.cpp
    command_handlers/user_command_handlers.cpp
    command_handlers/message_command_handlers.cpp
    command_handlers/friend_command_handlers.cpp
    command_handlers/group_command_handlers.cpp
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
)

target_compile_definitions(im_gateway_core
    PUBLIC
        CPPHTTPLIB_LISTEN_BACKLOG=${MYCHAT_HTTP_LISTEN_BACKLOG}
)

target_include_directories(im_gateway_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/gateway_server
        ${CMAKE_CURRENT_SOURCE_DIR}/command_handlers
        ${CMAKE_CURRENT_SOURCE_DIR}/connection_manager
        ${CMAKE_CURRENT_SOURCE_DIR}/router
        ${CMAKE_CURRENT_SOURCE_DIR}/message_processor
        ${CMAKE_CURRENT_SOURCE_DIR}/http
        ${CMAKE_CURRENT_SOURCE_DIR}/service_adapters
        ${CMAKE_CURRENT_SOURCE_DIR}/push
        ${CMAKE_CURRENT_SOURCE_DIR}/ws
)

# 创建命名空间别名
add_library(im::gateway_core ALIAS im_gateway_core)

# User Gateway adapter — 仅当 User Service 可用时编译和链接
if(TARGET im::user_service)
    target_link_libraries(im_gateway_core PUBLIC im::user_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_USER_HTTP)
    message(STATUS "User gateway adapter enabled (im::user_service available)")
else()
    message(STATUS "User gateway adapter disabled (im::user_service not available)")
endif()

# Remote User service adapter — explicit gRPC path only.
if(TARGET im::user_grpc_service)
    target_sources(im_gateway_core PRIVATE service_adapters/remote_user_service_adapter.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::user_grpc_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_REMOTE_USER_ADAPTER)
    message(STATUS "Remote User service adapter enabled (im::user_grpc_service available)")
else()
    message(STATUS "Remote User service adapter disabled (im::user_grpc_service not available)")
endif()

# Message Gateway adapter — 仅当 Message Service 可用时编译和链接
if(TARGET im::message_service)
    target_link_libraries(im_gateway_core PUBLIC im::message_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_MESSAGE_HTTP)
    message(STATUS "Message gateway adapter enabled (im::message_service available)")
else()
    message(STATUS "Message gateway adapter disabled (im::message_service not available)")
endif()

# Remote Message service adapter — explicit gRPC path only.
if(TARGET im::message_grpc_service)
    target_sources(im_gateway_core PRIVATE service_adapters/remote_message_service_adapter.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::message_grpc_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_REMOTE_MESSAGE_ADAPTER)
    message(STATUS "Remote Message service adapter enabled (im::message_grpc_service available)")
else()
    message(STATUS "Remote Message service adapter disabled (im::message_grpc_service not available)")
endif()

# PushService — requires Message Service plus the service-owned Push boundary.
if(TARGET im::message_service AND TARGET im::push_service)
    target_sources(im_gateway_core PRIVATE push/push_service.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::push_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_PUSH_SERVICE)
    message(STATUS "PushService enabled (im::message_service + im::push_service available)")
else()
    message(STATUS "PushService disabled (requires im::message_service + im::push_service)")
endif()

# Remote PushNotifier client — explicit gRPC path only.
if(TARGET im::push_grpc_service)
    target_sources(im_gateway_core PRIVATE
        push/remote_push_notifier.cpp
        push/gateway_push_delivery_service.cpp
    )
    target_link_libraries(im_gateway_core PUBLIC im::push_grpc_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_REMOTE_PUSH_NOTIFIER)
    message(STATUS "Remote PushNotifier enabled (im::push_grpc_service available)")
else()
    message(STATUS "Remote PushNotifier disabled (im::push_grpc_service not available)")
endif()

# Message WS Handler — requires Message Service plus PushNotifier boundary.
if(TARGET im::message_service AND TARGET im::push_service)
    target_sources(im_gateway_core PRIVATE ws/message_ws_handler.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::push_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_MESSAGE_WS)
    message(STATUS "Message WS handler enabled (im::message_service + im::push_service available)")
else()
    message(STATUS "Message WS handler disabled (requires im::message_service + im::push_service)")
endif()
# Friend Gateway adapter — 仅当 Friend Service 可用时编译和链接
if(TARGET im::friend_service)
    target_link_libraries(im_gateway_core PUBLIC im::friend_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_FRIEND_HTTP)
    message(STATUS "Friend gateway adapter enabled (im::friend_service available)")
else()
    message(STATUS "Friend gateway adapter disabled (im::friend_service not available)")
endif()

# Remote Friend service adapter — explicit gRPC path only.
if(TARGET im::friend_grpc_service)
    target_sources(im_gateway_core PRIVATE service_adapters/remote_friend_service_adapter.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::friend_grpc_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_REMOTE_FRIEND_ADAPTER)
    message(STATUS "Remote Friend service adapter enabled (im::friend_grpc_service available)")
else()
    message(STATUS "Remote Friend service adapter disabled (im::friend_grpc_service not available)")
endif()

# Group Gateway adapter — 仅当 Group Service 可用时编译和链接
if(TARGET im::group_service)
    target_link_libraries(im_gateway_core PUBLIC im::group_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_GROUP_HTTP)
    message(STATUS "Group gateway adapter enabled (im::group_service available)")
else()
    message(STATUS "Group gateway adapter disabled (im::group_service not available)")
endif()

# Remote Group service adapter — explicit gRPC path only.
if(TARGET im::group_grpc_service)
    target_sources(im_gateway_core PRIVATE service_adapters/remote_group_service_adapter.cpp)
    target_link_libraries(im_gateway_core PUBLIC im::group_grpc_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_REMOTE_GROUP_ADAPTER)
    message(STATUS "Remote Group service adapter enabled (im::group_grpc_service available)")
else()
    message(STATUS "Remote Group service adapter disabled (im::group_grpc_service not available)")
endif()

# Group Message Gateway entrypoint — requires Group/Message services plus PushNotifier boundary.
if(TARGET im::group_service AND TARGET im::message_service AND TARGET im::push_service)
    target_link_libraries(im_gateway_core PUBLIC im::push_service)
    target_compile_definitions(im_gateway_core PUBLIC IM_ENABLE_GROUP_MESSAGE_HTTP)
    message(STATUS "Group Message gateway entrypoint enabled (im::group_service + im::message_service + im::push_service available)")
else()
    message(STATUS "Group Message gateway entrypoint disabled (requires im::group_service + im::message_service + im::push_service)")
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
