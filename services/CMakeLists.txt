# services/CMakeLists.txt

# 推送服务模块 — 当前提供 PushNotifier 边界和服务层 fanout policy；
# WebSocket 发送 runtime adapter 暂在 gateway/push。
if(MYCHAT_BUILD_PUSH_SERVICE OR MYCHAT_BUILD_PUSH_GRPC_SERVICE)
    add_subdirectory(push)
    message(STATUS "Push service module (im_push_service) is enabled.")
else()
    message(STATUS "Push service module is disabled (MYCHAT_BUILD_PUSH_SERVICE=OFF).")
endif()

# Codec服务库 — 仅在 MYCHAT_BUILD_CODEC_SERVICE=ON 时构建。
if(MYCHAT_BUILD_CODEC_SERVICE)
    if(gRPC_FOUND AND TARGET gRPC::grpc++ AND MYCHAT_GRPC_CPP_PLUGIN)
        set_source_files_properties(
            ${MYCHAT_CODEC_GRPC_GENERATED_CC}
            ${MYCHAT_CODEC_GRPC_GENERATED_H}
            PROPERTIES GENERATED TRUE)

        add_library(im_codec_service STATIC
            ${MYCHAT_CODEC_GRPC_GENERATED_CC}
            ${MYCHAT_CODEC_GRPC_GENERATED_H}
        )
        add_dependencies(im_codec_service generate_codec_grpc)

        target_link_libraries(im_codec_service
            PUBLIC
                im::proto
                gRPC::grpc++
                gRPC::grpc
                ${Protobuf_LIBRARIES}
                utf8_range::utf8_range utf8_range::utf8_validity
        )

        target_include_directories(im_codec_service
            PUBLIC
                ${CMAKE_SOURCE_DIR}/common/proto
        )

        add_library(im::codec_service ALIAS im_codec_service)
        message(STATUS "Codec service (im_codec_service) is enabled.")
    else()
        message(FATAL_ERROR
            "MYCHAT_BUILD_CODEC_SERVICE=ON was set, but gRPC dependencies are not available. "
            "Install gRPC via vcpkg feature codec-grpc and ensure gRPC++ targets plus "
            "grpc_cpp_plugin are found.")
    endif()
else()
    message(STATUS "Codec service is disabled. Set MYCHAT_BUILD_CODEC_SERVICE=ON and enable vcpkg feature codec-grpc to build it.")
endif()

# 用户服务库 — gated on MYCHAT_BUILD_USER_SERVICE/MYCHAT_BUILD_USER_GRPC_SERVICE and ODB persistence layer.
if((MYCHAT_BUILD_USER_SERVICE OR MYCHAT_BUILD_USER_GRPC_SERVICE) AND TARGET im::user_odb)
    add_subdirectory(user)
    message(STATUS "User service (im_user_service) is enabled.")
elseif(MYCHAT_BUILD_USER_SERVICE OR MYCHAT_BUILD_USER_GRPC_SERVICE)
    message(STATUS "User service is disabled (MYCHAT_BUILD_USER_SERVICE=ON but im::user_odb not available). "
        "Enable MYCHAT_BUILD_PGSQL_ODB=ON and ensure ODB 2.5.0 runtime is available.")
else()
    message(STATUS "User service is disabled (MYCHAT_BUILD_USER_SERVICE=OFF).")
endif()

# 好友服务 -- gated on MYCHAT_BUILD_FRIEND_SERVICE and ODB persistence layer.
if(MYCHAT_BUILD_FRIEND_SERVICE AND TARGET im::friend_odb AND TARGET im::user_service)
    add_subdirectory(friend)
    message(STATUS "Friend service (im_friend_service) is enabled.")
elseif(MYCHAT_BUILD_FRIEND_SERVICE)
    if(NOT TARGET im::friend_odb)
        message(STATUS "Friend service is disabled (MYCHAT_BUILD_FRIEND_SERVICE=ON but im::friend_odb not available).")
    endif()
    if(NOT TARGET im::user_service)
        message(STATUS "Friend service is disabled (MYCHAT_BUILD_FRIEND_SERVICE=ON but im::user_service not available).")
    endif()
else()
    message(STATUS "Friend service is disabled (MYCHAT_BUILD_FRIEND_SERVICE=OFF).")
endif()

# 群组服务库 — gated on MYCHAT_BUILD_GROUP_SERVICE and ODB persistence layer.
if(MYCHAT_BUILD_GROUP_SERVICE AND TARGET im::group_odb AND TARGET im::user_service)
    add_subdirectory(group)
    message(STATUS "Group service (im_group_service) is enabled.")
elseif(MYCHAT_BUILD_GROUP_SERVICE)
    if(NOT TARGET im::group_odb)
        message(STATUS "Group service is disabled (MYCHAT_BUILD_GROUP_SERVICE=ON but im::group_odb not available).")
    endif()
    if(NOT TARGET im::user_service)
        message(STATUS "Group service is disabled (MYCHAT_BUILD_GROUP_SERVICE=ON but im::user_service not available).")
    endif()
else()
    message(STATUS "Group service is disabled (MYCHAT_BUILD_GROUP_SERVICE=OFF).")
endif()

# 消息服务库 — gated on MYCHAT_BUILD_MESSAGE_SERVICE and ODB persistence layer.
if(MYCHAT_BUILD_MESSAGE_SERVICE AND TARGET im::message_odb)
    add_subdirectory(message)
    message(STATUS "Message service (im_message_service) is enabled.")
elseif(MYCHAT_BUILD_MESSAGE_SERVICE)
    message(STATUS "Message service is disabled (MYCHAT_BUILD_MESSAGE_SERVICE=ON but im::message_odb not available). "
        "Enable MYCHAT_BUILD_PGSQL_ODB=ON and ensure ODB 2.5.0 runtime is available.")
else()
    message(STATUS "Message service is disabled (MYCHAT_BUILD_MESSAGE_SERVICE=OFF).")
endif()
