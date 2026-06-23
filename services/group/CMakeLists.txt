# services/group/CMakeLists.txt
#
# Group Service — create group, join/leave, list members.
# Built only when ODB persistence layer (im::group_odb) and User Service
# (im::user_service) are both available.

add_library(im_group_service STATIC
    group_repository.cpp
    group_packet_dispatcher.cpp
    group_service.cpp
    group_message_repository.cpp
    group_message_service.cpp
)

target_link_libraries(im_group_service
    PUBLIC
        im::group_odb
        im::group_message_odb
        im::proto
        im::user_service
    PRIVATE
        im::utils
        nlohmann_json::nlohmann_json
)

target_include_directories(im_group_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(im::group_service ALIAS im_group_service)

if(MYCHAT_BUILD_GROUP_GRPC_SERVICE)
    if(gRPC_FOUND AND TARGET gRPC::grpc++ AND MYCHAT_GRPC_CPP_PLUGIN)
        set_source_files_properties(
            ${MYCHAT_GROUP_GRPC_GENERATED_CC}
            ${MYCHAT_GROUP_GRPC_GENERATED_H}
            PROPERTIES GENERATED TRUE)

        add_library(im_group_grpc_service STATIC
            group_grpc_service.cpp
            group_server_app.cpp
            group_server_app.hpp
            ${MYCHAT_GROUP_GRPC_GENERATED_CC}
            ${MYCHAT_GROUP_GRPC_GENERATED_H}
        )
        add_dependencies(im_group_grpc_service generate_group_grpc)

        target_link_libraries(im_group_grpc_service
            PUBLIC
                im::group_service
                im::proto
                gRPC::grpc++
                gRPC::grpc
                ${Protobuf_LIBRARIES}
                utf8_range::utf8_range utf8_range::utf8_validity
            PRIVATE
                im::utils
        )

        target_include_directories(im_group_grpc_service
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/common/proto
        )

        add_library(im::group_grpc_service ALIAS im_group_grpc_service)

        add_executable(group_server
            group_server_main.cpp
        )

        target_link_libraries(group_server
            PRIVATE
                im::group_grpc_service
                im::utils
        )

        target_compile_features(group_server PRIVATE cxx_std_20)
    else()
        message(FATAL_ERROR
            "MYCHAT_BUILD_GROUP_GRPC_SERVICE=ON was set, but gRPC dependencies are not available. "
            "Install gRPC via vcpkg feature codec-grpc and ensure grpc_cpp_plugin is found.")
    endif()
endif()
