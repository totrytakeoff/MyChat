# services/CMakeLists.txt

# 获取所需依赖（仅当需要构建对应服务时）
if(MYCHAT_BUILD_CODEC_SERVICE)
    find_package(gRPC CONFIG QUIET)
endif()

# Codec服务库 — 仅在 MYCHAT_BUILD_CODEC_SERVICE=ON 时构建。
if(MYCHAT_BUILD_CODEC_SERVICE)
    if(gRPC_FOUND AND TARGET gRPC::grpc++)
        add_library(im_codec_service STATIC
            codec/codec_service.pb.cc
            codec/codec_service.grpc.pb.cc
            codec/base.pb.cc
        )

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
                ${CMAKE_CURRENT_SOURCE_DIR}/codec
        )

        add_library(im::codec_service ALIAS im_codec_service)
        message(STATUS "Codec service (im_codec_service) is enabled.")
    else()
        message(FATAL_ERROR
            "MYCHAT_BUILD_CODEC_SERVICE=ON was set, but gRPC dependencies are not available. "
            "Install gRPC via vcpkg and ensure gRPC++ targets are found.")
    endif()
else()
    message(STATUS "Codec service is disabled. Set MYCHAT_BUILD_CODEC_SERVICE=ON and install gRPC to enable.")
endif()

# 用户服务库 — gated on MYCHAT_BUILD_USER_SERVICE and ODB persistence layer.
if(MYCHAT_BUILD_USER_SERVICE AND TARGET im::user_odb)
    add_subdirectory(user)
    message(STATUS "User service (im_user_service) is enabled.")
elseif(MYCHAT_BUILD_USER_SERVICE)
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

# 推送服务库
# add_library(im_push_service STATIC
#    services/push/*.cpp
# )
#
# target_link_libraries(im_push_service
#    PUBLIC
#        im::proto
#        im::database
#        im::utils
# )
#
# target_include_directories(im_push_service
#    PUBLIC
#        ${CMAKE_CURRENT_SOURCE_DIR}/push
# )
#
# # 创建命名空间别名
# add_library(im::push_service ALIAS im_push_service)