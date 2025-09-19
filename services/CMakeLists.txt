# services/CMakeLists.txt

# Codec服务库
add_library(im_codec_service STATIC
    codec/codec_service.pb.cc
    codec/codec_service.grpc.pb.cc
    codec/base.pb.cc
)

target_link_libraries(im_codec_service
    PUBLIC
        im::proto
        ${Protobuf_LIBRARIES}
        utf8_range::utf8_range utf8_range::utf8_validity
)

target_include_directories(im_codec_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/codec
)

# 创建命名空间别名
add_library(im::codec_service ALIAS im_codec_service)

# 用户服务库
file(GLOB_RECURSE USER_SOURCES "user/*.cpp")
if(USER_SOURCES)
    add_library(im_user_service STATIC ${USER_SOURCES})
    
    target_link_libraries(im_user_service
        PUBLIC
            im::proto
            im::database
            im::utils
    )
    
    target_include_directories(im_user_service
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/user
    )
    
    # 创建命名空间别名
    add_library(im::user_service ALIAS im_user_service)
endif()

# 好友服务库
file(GLOB_RECURSE FRIEND_SOURCES "friend/*.cpp")
if(FRIEND_SOURCES)
    add_library(im_friend_service STATIC ${FRIEND_SOURCES})
    
    target_link_libraries(im_friend_service
        PUBLIC
            im::proto
            im::database
            im::utils
    )
    
    target_include_directories(im_friend_service
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/friend
    )
    
    # 创建命名空间别名
    add_library(im::friend_service ALIAS im_friend_service)
endif()

# 群组服务库
# add_library(im_group_service STATIC
#    services/group/*.cpp
# )
#
# target_link_libraries(im_group_service
#    PUBLIC
#        im::proto
#        im::database
#        im::utils
# )
#
# target_include_directories(im_group_service
#    PUBLIC
#        ${CMAKE_CURRENT_SOURCE_DIR}/group
# )
#
# # 创建命名空间别名
# add_library(im::group_service ALIAS im_group_service)

# 消息服务库
# add_library(im_message_service STATIC
#    services/message/*.cpp
# )
#
# target_link_libraries(im_message_service
#    PUBLIC
#        im::proto
#        im::database
#        im::utils
# )
#
# target_include_directories(im_message_service
#    PUBLIC
#        ${CMAKE_CURRENT_SOURCE_DIR}/message
# )
#
# # 创建命名空间别名
# add_library(im::message_service ALIAS im_message_service)

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