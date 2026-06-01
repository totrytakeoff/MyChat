# services/user/CMakeLists.txt
#
# User Service Core — register, login, profile operations.
# Built only when ODB persistence layer (im::user_odb) is available.

add_library(im_user_service STATIC
    password_hasher.cpp
    user_repository.cpp
    user_service.cpp
)

target_link_libraries(im_user_service
    PUBLIC
        im::user_odb
        OpenSSL::Crypto
    PRIVATE
        im::utils
)

target_include_directories(im_user_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(im::user_service ALIAS im_user_service)
