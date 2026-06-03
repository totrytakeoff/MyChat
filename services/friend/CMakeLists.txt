# services/friend/CMakeLists.txt
#
# Friend Service — friend request, accept/reject, list friends.
# Built only when ODB persistence layer (im::friend_odb) and User Service
# (im::user_service) are both available.

add_library(im_friend_service STATIC
    friend_repository.cpp
    friend_service.cpp
)

target_link_libraries(im_friend_service
    PUBLIC
        im::friend_odb
        im::user_service
    PRIVATE
        im::utils
)

target_include_directories(im_friend_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(im::friend_service ALIAS im_friend_service)
