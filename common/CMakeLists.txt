# common/CMakeLists.txt

# 网络库
add_library(im_network STATIC
    network/IOService_pool.cpp
    network/tcp_server.cpp
    network/tcp_session.cpp
    network/websocket_server.cpp
    network/websocket_session.cpp
    network/protobuf_codec.cpp
    network/packet_error_builder.cpp
)

target_compile_options(im_network PRIVATE -fcoroutines)

target_link_libraries(im_network
    PUBLIC
        Threads::Threads
        Boost::boost
        OpenSSL::SSL
        OpenSSL::Crypto
        ${Protobuf_LIBRARIES}
        nlohmann_json::nlohmann_json
)

target_include_directories(im_network
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/network
)

# 创建命名空间别名
add_library(im::network ALIAS im_network)

# 工具库
add_library(im_utils STATIC
    utils/log_manager.cpp
    utils/coroutine_manager.cpp
    utils/service_identity.cpp
    utils/thread_pool.cpp
    utils/config_mgr.hpp
    utils/signal_handler.hpp
    utils/cli_parser.hpp
    utils/connection_pool.hpp
    utils/global.hpp
    utils/http_utils.hpp
    utils/memory_pool.hpp
    utils/singleton.hpp
    utils/time_utils.hpp
)

target_compile_options(im_utils PRIVATE -fcoroutines)

target_link_libraries(im_utils
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

target_include_directories(im_utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# 创建命名空间别名
add_library(im::utils ALIAS im_utils)

# 数据库库
if(hiredis_FOUND)
    add_library(im_database STATIC
        database/redis/redis_mgr.cpp
    )

    target_link_libraries(im_database
        PUBLIC
            im::utils
            hiredis::hiredis
    )

    target_include_directories(im_database
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/database
    )

    # 创建命名空间别名
    add_library(im::database ALIAS im_database)
else()
    message(STATUS "hiredis was not found; im::database target is not built.")
endif()

# PostgreSQL/ODB 数据库库
if(MYCHAT_BUILD_PGSQL_ODB AND MYCHAT_HAS_ODB_RUNTIME)
    add_library(im_pgsql STATIC
        database/pgsql/pgsql_conn.cpp
    )

    target_include_directories(im_pgsql
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/database
    )

    target_link_libraries(im_pgsql
        PUBLIC
            im::utils
            odb::libodb
            odb::libodb-pgsql
            nlohmann_json::nlohmann_json
    )

    add_library(im::pgsql ALIAS im_pgsql)
    message(STATUS "PostgreSQL/ODB library (im_pgsql) is enabled.")
elseif(MYCHAT_BUILD_PGSQL_ODB)
    message(WARNING "MYCHAT_BUILD_PGSQL_ODB=ON but ODB runtime libraries not found. "
        "Install ODB runtime libraries by passing -DVCPKG_MANIFEST_FEATURES=pgsql-odb. "
        "The ODB compiler (odb) is a separate tool from the runtime libraries.")
else()
    message(STATUS "PostgreSQL/ODB targets are disabled. Set MYCHAT_BUILD_PGSQL_ODB=ON to enable.")
endif()

# Protobuf库
set_source_files_properties(
    ${MYCHAT_COMMON_PROTO_GENERATED_CC}
    ${MYCHAT_COMMON_PROTO_GENERATED_H}
    PROPERTIES GENERATED TRUE)

add_library(im_proto STATIC
    ${MYCHAT_COMMON_PROTO_GENERATED_CC}
)

add_dependencies(im_proto generate_common_proto)

target_link_libraries(im_proto
    PUBLIC
        ${Protobuf_LIBRARIES}
        ${ABSL_LIBRARIES}
        utf8_range::utf8_range utf8_range::utf8_validity
)

target_include_directories(im_proto
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
        ${Protobuf_INCLUDE_DIRS}
)

# 创建命名空间别名
add_library(im::proto ALIAS im_proto)
