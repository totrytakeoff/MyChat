# # test/network/CMakeLists.txt

# # protobuf 编码解码测试
# add_executable(protobufcodec_test
#     test_protobufcodec.cpp
#     test_message.pb.cc
# )

# target_link_libraries(protobufcodec_test
#     PRIVATE
#         GTest::gtest
#         GTest::gtest_main
#         im::network  # 使用新的组件库
#         im::utils    # 使用新的组件库
#         im::proto    # 使用新的组件库
#         Threads::Threads
#         Boost::system
#         spdlog::spdlog
#         ${Protobuf_LIBRARIES}
#         utf8_range::utf8_range utf8_range::utf8_validity
# )

# target_include_directories(protobufcodec_test PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
# )

# # WebSocket 测试
# add_executable(websocket_test
#     test_websocket.cpp
# )

# target_link_libraries(websocket_test
#     PRIVATE
#         GTest::gtest
#         GTest::gtest_main
#         im::network  # 使用新的组件库
#         im::utils    # 使用新的组件库
#         absl::log
#         absl::log_internal_check_op  # 新增关键组件
#         absl::log_internal_message
#         Threads::Threads
#         Boost::system
#         spdlog::spdlog
#         OpenSSL::SSL
#         OpenSSL::Crypto
# )

# target_include_directories(websocket_test PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}
# )

# # 添加测试
# add_test(NAME ProtobufCodecTests COMMAND protobufcodec_test)
# add_test(NAME WebSocketTests COMMAND websocket_test)