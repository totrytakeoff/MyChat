# services/message/CMakeLists.txt
#
# Message Service Core — send, history, offline pull, delivered marking.
# Built only when ODB persistence layer (im::message_odb) is available.

add_library(im_message_service STATIC
    message_repository.cpp
    message_service.cpp
)

target_link_libraries(im_message_service
    PUBLIC
        im::message_odb
    PRIVATE
        im::utils
)

target_include_directories(im_message_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(im::message_service ALIAS im_message_service)
