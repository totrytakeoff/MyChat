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

if(MYCHAT_BUILD_FRIEND_GRPC_SERVICE)
    if(gRPC_FOUND AND TARGET gRPC::grpc++ AND MYCHAT_GRPC_CPP_PLUGIN)
        set_source_files_properties(
            ${MYCHAT_FRIEND_GRPC_GENERATED_CC}
            ${MYCHAT_FRIEND_GRPC_GENERATED_H}
            PROPERTIES GENERATED TRUE)

        add_library(im_friend_grpc_service STATIC
            friend_grpc_service.cpp
            friend_grpc_service.hpp
            friend_server_app.cpp
            friend_server_app.hpp
            ${MYCHAT_FRIEND_GRPC_GENERATED_CC}
            ${MYCHAT_FRIEND_GRPC_GENERATED_H}
        )
        add_dependencies(im_friend_grpc_service generate_friend_grpc)

        target_include_directories(im_friend_grpc_service
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/common/proto
        )

        target_link_libraries(im_friend_grpc_service
            PUBLIC
                im::friend_service
                im::proto
                gRPC::grpc++
                gRPC::grpc
                ${Protobuf_LIBRARIES}
                utf8_range::utf8_range utf8_range::utf8_validity
        )

        add_library(im::friend_grpc_service ALIAS im_friend_grpc_service)

        add_executable(friend_server
            friend_server_main.cpp
        )

        target_link_libraries(friend_server
            PRIVATE
                im::friend_grpc_service
                im::utils
        )
    else()
        message(FATAL_ERROR
            "MYCHAT_BUILD_FRIEND_GRPC_SERVICE=ON was set, but gRPC dependencies are not available. "
            "Install gRPC via vcpkg feature codec-grpc and ensure gRPC++ targets plus "
            "grpc_cpp_plugin are found.")
    endif()
endif()
