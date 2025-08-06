cmake_minimum_required(VERSION 3.10)
project(network_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 vcpkg 工具链（确保在所有 find_package 之前）
set(CMAKE_TOOLCHAIN_FILE "/home/myself/vcpkg/scripts/buildsystems/vcpkg.cmake" 
    CACHE STRING "Vcpkg toolchain file")

# 查找所需的库
find_package(Threads REQUIRED)
find_package(GTest REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)

# # 添加测试源文件
# set(TEST_SOURCES
#     test_server.cpp
#     test_session.cpp
#     test_io_service_pool.cpp
# )

# # 添加网络库源文件
# set(NETWORK_SOURCES
#     ../../common/network/tcp_server.cpp
#     ../../common/network/tcp_session.cpp
#     ../../common/network/IOService_pool.cpp
#     ../../common/utils/log_manager.cpp
#     ../../common/utils/thread_pool.cpp
# )




# # 创建测试可执行文件
# add_executable(network_tests ${TEST_SOURCES} ${NETWORK_SOURCES})

# # 创建简化测试可执行文件
# add_executable(network_simple_tests test_simple_server.cpp ${NETWORK_SOURCES})




# # 链接所需的库
# target_link_libraries(network_tests 
#     ${GTEST_BOTH_LIBRARIES}
#     ${CMAKE_THREAD_LIBS_INIT}
#     Boost::system
#     spdlog::spdlog
#     stdc++fs
# )

# target_link_libraries(network_simple_tests 
#     ${GTEST_BOTH_LIBRARIES}
#     ${CMAKE_THREAD_LIBS_INIT}
#     Boost::system
#     spdlog::spdlog
#     stdc++fs
# )




# # 包含头文件目录
# target_include_directories(network_tests PRIVATE 
#     ../../common
#     ../../common/network
#     ../../common/utils
# )

# target_include_directories(network_simple_tests PRIVATE 
#     ../../common
#     ../../common/network
#     ../../common/utils
# )

# # 启用C++17
# target_compile_features(network_tests PRIVATE cxx_std_17)
# target_compile_features(network_simple_tests PRIVATE cxx_std_17)

# # 添加测试
# add_test(NAME NetworkTests COMMAND network_tests)

# add_test(NAME NetworkSimpleTests COMMAND network_simple_tests)


# protobuf 编码解码测试

set(PROTOBUF_SOURCES
    ../../common/proto/base.pb.cc
    ./test_message.pb.cc
    ../../common/network/protobuf_codec.cpp
    ../../common/utils/log_manager.cpp
)

add_executable(protobufcodec_test test_protobufcodec.cpp ${PROTOBUF_SOURCES})

target_link_libraries(protobufcodec_test 
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    Boost::system
    spdlog::spdlog
    stdc++fs
    protobuf::libprotobuf
)

target_include_directories(protobufcodec_test PRIVATE 
    ../../common
    ../../common/network
    ../../common/utils
)


target_compile_features(protobufcodec_test PRIVATE cxx_std_17)

add_test(NAME ProtobufCodecTests COMMAND protobufcodec_test)

# WebSocket 测试
set(WEBSOCKET_SOURCES
    ../../common/network/websocket_server.cpp
    ../../common/network/websocket_session.cpp
    ../../common/utils/log_manager.cpp
    ../../common/utils/thread_pool.cpp
)

add_executable(websocket_test test_websocket.cpp ${WEBSOCKET_SOURCES})

target_link_libraries(websocket_test 
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    Boost::system
    spdlog::spdlog
    stdc++fs
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(websocket_test PRIVATE 
    ../../common
    ../../common/network
    ../../common/utils
)

target_compile_features(websocket_test PRIVATE cxx_std_17)

add_test(NAME WebSocketTests COMMAND websocket_test)