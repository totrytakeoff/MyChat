# services/odb/CMakeLists.txt
#
# ODB-generated user entity target. Only built when the ODB runtime
# libraries are available (MYCHAT_BUILD_PGSQL_ODB=ON + odb found).
#
# IMPORTANT: The generated source files (user-odb.cxx / .hxx / .ixx)
# are produced by running the `odb` compiler manually. If these files
# are missing, CMake will fail with a clear message telling the
# developer to run the documented generation command. See
# docs/devlog/phase3_odb_user_persistence.md for the exact command.

set(ODB_USER_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(ODB_USER_GENERATED_SOURCES
    "${ODB_USER_GENERATED_DIR}/user-odb.cxx"
)
set(ODB_USER_GENERATED_HEADERS
    "${ODB_USER_GENERATED_DIR}/user-odb.hxx"
    "${ODB_USER_GENERATED_DIR}/user-odb.ixx"
)

# Verify generated files exist; fail with a helpful message if not.
foreach(_f ${ODB_USER_GENERATED_SOURCES} ${ODB_USER_GENERATED_HEADERS})
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR
            "Generated ODB file '${_f}' not found.\n"
            "Run the ODB compiler from the project root:\n"
            "  odb -I<VCPKG_INCLUDE_DIR> \\\n"
            "      --database pgsql --std c++20 --generate-query\n"
            "      --generate-schema --schema-format sql\n"
            "      --output-dir services/odb/generated\n"
            "      --hxx-suffix .hxx --ixx-suffix .ixx\n"
            "      --cxx-suffix .cxx --sql-suffix .sql\n"
            "      services/odb/user.hpp\n"
            "\n"
            "Also run the ODB runtime build script if needed:\n"
            "  scripts/build_odb_runtime_2_5.sh\n"
            "See docs/devlog/phase3_odb_user_persistence.md for details.")
    endif()
endforeach()

add_library(im_user_odb STATIC
    ${ODB_USER_GENERATED_SOURCES}
)

target_include_directories(im_user_odb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ODB_USER_GENERATED_DIR}
)

target_link_libraries(im_user_odb
    PUBLIC
        odb::libodb
        odb::libodb-pgsql
)

add_library(im::user_odb ALIAS im_user_odb)

message(STATUS "ODB user persistence model (im_user_odb) is enabled.")

# --- Message ODB entity ---

set(ODB_MSG_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(ODB_MSG_GENERATED_SOURCES
    "${ODB_MSG_GENERATED_DIR}/message-odb.cxx"
)
set(ODB_MSG_GENERATED_HEADERS
    "${ODB_MSG_GENERATED_DIR}/message-odb.hxx"
    "${ODB_MSG_GENERATED_DIR}/message-odb.ixx"
)

foreach(_f ${ODB_MSG_GENERATED_SOURCES} ${ODB_MSG_GENERATED_HEADERS})
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR
            "Generated ODB file '${_f}' not found.\n"
            "Run the ODB compiler from the project root:\n"
            "  odb -I<VCPKG_INCLUDE_DIR> \\\n"
            "      --database pgsql --std c++20 --generate-query\n"
            "      --generate-schema --schema-format sql\n"
            "      --output-dir services/odb/generated\n"
            "      --hxx-suffix .hxx --ixx-suffix .ixx\n"
            "      --cxx-suffix .cxx --sql-suffix .sql\n"
            "      services/odb/message.hpp")
    endif()
endforeach()

add_library(im_message_odb STATIC
    ${ODB_MSG_GENERATED_SOURCES}
)

target_include_directories(im_message_odb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ODB_MSG_GENERATED_DIR}
)

target_link_libraries(im_message_odb
    PUBLIC
        odb::libodb
        odb::libodb-pgsql
)

add_library(im::message_odb ALIAS im_message_odb)

message(STATUS "ODB message persistence model (im_message_odb) is enabled.")

# --- Friend ODB entity ---

set(ODB_FRIEND_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(ODB_FRIEND_GENERATED_SOURCES
    "${ODB_FRIEND_GENERATED_DIR}/friend-odb.cxx"
)
set(ODB_FRIEND_GENERATED_HEADERS
    "${ODB_FRIEND_GENERATED_DIR}/friend-odb.hxx"
    "${ODB_FRIEND_GENERATED_DIR}/friend-odb.ixx"
)

foreach(_f ${ODB_FRIEND_GENERATED_SOURCES} ${ODB_FRIEND_GENERATED_HEADERS})
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR
            "Generated ODB file '${_f}' not found.\n"
            "Run the ODB compiler from the project root:\n"
            "  odb -I<VCPKG_INCLUDE_DIR> \\\n"
            "      --database pgsql --std c++20 --generate-query\n"
            "      --generate-schema --schema-format sql\n"
            "      --output-dir services/odb/generated\n"
            "      --hxx-suffix .hxx --ixx-suffix .ixx\n"
            "      --cxx-suffix .cxx --sql-suffix .sql\n"
            "      services/odb/friend.hpp")
    endif()
endforeach()

add_library(im_friend_odb STATIC
    ${ODB_FRIEND_GENERATED_SOURCES}
)

target_include_directories(im_friend_odb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ODB_FRIEND_GENERATED_DIR}
)

target_link_libraries(im_friend_odb
    PUBLIC
        odb::libodb
        odb::libodb-pgsql
)

add_library(im::friend_odb ALIAS im_friend_odb)

message(STATUS "ODB friend persistence model (im_friend_odb) is enabled.")

# --- Group ODB entity ---

set(ODB_GROUP_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(ODB_GROUP_GENERATED_SOURCES
    "${ODB_GROUP_GENERATED_DIR}/group-odb.cxx"
)
set(ODB_GROUP_GENERATED_HEADERS
    "${ODB_GROUP_GENERATED_DIR}/group-odb.hxx"
    "${ODB_GROUP_GENERATED_DIR}/group-odb.ixx"
)

foreach(_f ${ODB_GROUP_GENERATED_SOURCES} ${ODB_GROUP_GENERATED_HEADERS})
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR
            "Generated ODB file '${_f}' not found.\n"
            "Run the ODB compiler from the project root:\n"
            "  odb -I<VCPKG_INCLUDE_DIR> \\\n"
            "      --database pgsql --std c++20 --generate-query\n"
            "      --generate-schema --schema-format sql\n"
            "      --output-dir services/odb/generated\n"
            "      --hxx-suffix .hxx --ixx-suffix .ixx\n"
            "      --cxx-suffix .cxx --sql-suffix .sql\n"
            "      services/odb/group.hpp")
    endif()
endforeach()

add_library(im_group_odb STATIC
    ${ODB_GROUP_GENERATED_SOURCES}
)

target_include_directories(im_group_odb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ODB_GROUP_GENERATED_DIR}
)

target_link_libraries(im_group_odb
    PUBLIC
        odb::libodb
        odb::libodb-pgsql
)

add_library(im::group_odb ALIAS im_group_odb)

message(STATUS "ODB group persistence model (im_group_odb) is enabled.")

# --- GroupMessage ODB entity ---

set(ODB_GROUP_MSG_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(ODB_GROUP_MSG_GENERATED_SOURCES
    "${ODB_GROUP_MSG_GENERATED_DIR}/group_message-odb.cxx"
)
set(ODB_GROUP_MSG_GENERATED_HEADERS
    "${ODB_GROUP_MSG_GENERATED_DIR}/group_message-odb.hxx"
    "${ODB_GROUP_MSG_GENERATED_DIR}/group_message-odb.ixx"
)

foreach(_f ${ODB_GROUP_MSG_GENERATED_SOURCES} ${ODB_GROUP_MSG_GENERATED_HEADERS})
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR
            "Generated ODB file '${_f}' not found.\n"
            "Run the ODB compiler from the project root:\n"
            "  odb -I<VCPKG_INCLUDE_DIR> \\\n"
            "      --database pgsql --std c++20 --generate-query\n"
            "      --generate-schema --schema-format sql\n"
            "      --output-dir services/odb/generated\n"
            "      --hxx-suffix .hxx --ixx-suffix .ixx\n"
            "      --cxx-suffix .cxx --sql-suffix .sql\n"
            "      services/odb/group_message.hpp")
    endif()
endforeach()

add_library(im_group_message_odb STATIC
    ${ODB_GROUP_MSG_GENERATED_SOURCES}
)

target_include_directories(im_group_message_odb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ODB_GROUP_MSG_GENERATED_DIR}
)

target_link_libraries(im_group_message_odb
    PUBLIC
        odb::libodb
        odb::libodb-pgsql
)

add_library(im::group_message_odb ALIAS im_group_message_odb)

message(STATUS "ODB group message persistence model (im_group_message_odb) is enabled.")
