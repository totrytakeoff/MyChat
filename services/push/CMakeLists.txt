# services/push/CMakeLists.txt
#
# Push Service boundary, service-owned fanout policy logic, runtime core, and
# explicit gRPC server target. Gateway still owns WebSocket sessions today; the
# standalone push_server uses transitional adapters until a real cross-process
# session/payload/delivery channel is added.

add_library(im_push_service STATIC
    fanout_policy.cpp
    push_runtime.cpp
)

target_include_directories(im_push_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(im_push_service
    PUBLIC
        im::network
        im::proto
        im::utils
)

add_library(im::push_service ALIAS im_push_service)

if(MYCHAT_BUILD_PUSH_GRPC_SERVICE)
    if(gRPC_FOUND AND TARGET gRPC::grpc++ AND MYCHAT_GRPC_CPP_PLUGIN)
        set_source_files_properties(
            ${MYCHAT_PUSH_GRPC_GENERATED_CC}
            ${MYCHAT_PUSH_GRPC_GENERATED_H}
            PROPERTIES GENERATED TRUE)

        add_library(im_push_grpc_service STATIC
            push_grpc_service.cpp
            push_grpc_service.hpp
            push_server_adapters.cpp
            push_server_adapters.hpp
            push_server_app.cpp
            push_server_app.hpp
            ${MYCHAT_PUSH_GRPC_GENERATED_CC}
            ${MYCHAT_PUSH_GRPC_GENERATED_H}
        )
        add_dependencies(im_push_grpc_service generate_push_grpc)

        target_include_directories(im_push_grpc_service
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/common/proto
        )

        target_link_libraries(im_push_grpc_service
            PUBLIC
                im::push_service
                im::proto
                gRPC::grpc++
                gRPC::grpc
                ${Protobuf_LIBRARIES}
                utf8_range::utf8_range utf8_range::utf8_validity
        )

        add_library(im::push_grpc_service ALIAS im_push_grpc_service)

        add_executable(push_server
            push_server_main.cpp
        )

        target_link_libraries(push_server
            PRIVATE
                im::push_grpc_service
                im::utils
        )
    else()
        message(FATAL_ERROR
            "MYCHAT_BUILD_PUSH_GRPC_SERVICE=ON was set, but gRPC dependencies are not available. "
            "Install gRPC via vcpkg feature codec-grpc and ensure gRPC++ targets plus "
            "grpc_cpp_plugin are found.")
    endif()
endif()
