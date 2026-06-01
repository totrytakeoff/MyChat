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
            "  odb -I\$(scripts/find_vcpkg_include.sh) \\\n"
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
