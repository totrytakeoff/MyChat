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

if(MYCHAT_BUILD_USER_GRPC_SERVICE)
    if(gRPC_FOUND AND TARGET gRPC::grpc++ AND MYCHAT_GRPC_CPP_PLUGIN)
        set_source_files_properties(
            ${MYCHAT_USER_GRPC_GENERATED_CC}
            ${MYCHAT_USER_GRPC_GENERATED_H}
            PROPERTIES GENERATED TRUE)

        add_library(im_user_grpc_service STATIC
            user_grpc_service.cpp
            user_grpc_service.hpp
            user_server_app.cpp
            user_server_app.hpp
            ${MYCHAT_USER_GRPC_GENERATED_CC}
            ${MYCHAT_USER_GRPC_GENERATED_H}
        )
        add_dependencies(im_user_grpc_service generate_user_grpc)

        target_include_directories(im_user_grpc_service
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/common/proto
        )

        target_link_libraries(im_user_grpc_service
            PUBLIC
                im::user_service
                im::proto
                gRPC::grpc++
                gRPC::grpc
                ${Protobuf_LIBRARIES}
                utf8_range::utf8_range utf8_range::utf8_validity
        )

        add_library(im::user_grpc_service ALIAS im_user_grpc_service)

        add_executable(user_server
            user_server_main.cpp
        )

        target_link_libraries(user_server
            PRIVATE
                im::user_grpc_service
                im::utils
        )
    else()
        message(FATAL_ERROR
            "MYCHAT_BUILD_USER_GRPC_SERVICE=ON was set, but gRPC dependencies are not available. "
            "Install gRPC via vcpkg feature codec-grpc and ensure gRPC++ targets plus "
            "grpc_cpp_plugin are found.")
    endif()
endif()
