# common/CMakeLists.txt

# 网络库
add_library(im_network STATIC
    network/IOService_pool.cpp
    network/tcp_server.cpp
    network/tcp_session.cpp
    network/websocket_server.cpp
    network/websocket_session.cpp
    network/protobuf_codec.cpp
)

target_compile_options(im_network PRIVATE -fcoroutines)

target_link_libraries(im_network
    PUBLIC
        Threads::Threads
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        ${Protobuf_LIBRARIES}
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
add_library(im_database STATIC
    database/redis_mgr.cpp
)

target_link_libraries(im_database
    PUBLIC
        redis++::redis++_static
        nlohmann_json::nlohmann_json
)

target_include_directories(im_database
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/database
)

# 创建命名空间别名
add_library(im::database ALIAS im_database)

# Protobuf库
add_library(im_proto STATIC
    proto/base.pb.cc
    proto/command.pb.cc
    proto/user.pb.cc
    proto/friend.pb.cc
    proto/group.pb.cc
    proto/message.pb.cc
    proto/push.pb.cc
)

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