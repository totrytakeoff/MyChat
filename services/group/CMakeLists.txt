# services/group/CMakeLists.txt
#
# Group Service — create group, join/leave, list members.
# Built only when ODB persistence layer (im::group_odb) and User Service
# (im::user_service) are both available.

add_library(im_group_service STATIC
    group_repository.cpp
    group_service.cpp
    group_message_repository.cpp
    group_message_service.cpp
)

target_link_libraries(im_group_service
    PUBLIC
        im::group_odb
        im::group_message_odb
        im::user_service
    PRIVATE
        im::utils
)

target_include_directories(im_group_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(im::group_service ALIAS im_group_service)
