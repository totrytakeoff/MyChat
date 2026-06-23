# services/message/CMakeLists.txt
#
# Message Service Core — send, history, offline pull, delivered marking.
# Built only when ODB persistence layer (im::message_odb) is available.

add_library(im_message_service STATIC
    message_packet_dispatcher.cpp
    message_repository.cpp
    message_service.cpp
)

target_link_libraries(im_message_service
    PUBLIC
        im::message_odb
        im::proto
    PRIVATE
        im::utils
        nlohmann_json::nlohmann_json
)

target_include_directories(im_message_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(im::message_service ALIAS im_message_service)

if(MYCHAT_BUILD_MESSAGE_GRPC_SERVICE)
    if(gRPC_FOUND AND TARGET gRPC::grpc++ AND MYCHAT_GRPC_CPP_PLUGIN)
        set_source_files_properties(
            ${MYCHAT_MESSAGE_GRPC_GENERATED_CC}
            ${MYCHAT_MESSAGE_GRPC_GENERATED_H}
            PROPERTIES GENERATED TRUE)

        add_library(im_message_grpc_service STATIC
            message_grpc_service.cpp
            message_grpc_service.hpp
            message_server_app.cpp
            message_server_app.hpp
            ${MYCHAT_MESSAGE_GRPC_GENERATED_CC}
            ${MYCHAT_MESSAGE_GRPC_GENERATED_H}
        )
        add_dependencies(im_message_grpc_service generate_message_grpc)

        target_include_directories(im_message_grpc_service
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/common/proto
        )

        target_link_libraries(im_message_grpc_service
            PUBLIC
                im::message_service
                im::proto
                gRPC::grpc++
                gRPC::grpc
                ${Protobuf_LIBRARIES}
                utf8_range::utf8_range utf8_range::utf8_validity
        )

        add_library(im::message_grpc_service ALIAS im_message_grpc_service)

        add_executable(message_server
            message_server_main.cpp
        )

        target_link_libraries(message_server
            PRIVATE
                im::message_grpc_service
                im::utils
        )
    else()
        message(FATAL_ERROR
            "MYCHAT_BUILD_MESSAGE_GRPC_SERVICE=ON was set, but gRPC dependencies are not available. "
            "Install gRPC via vcpkg feature codec-grpc and ensure gRPC++ targets plus "
            "grpc_cpp_plugin are found.")
    endif()
endif()
