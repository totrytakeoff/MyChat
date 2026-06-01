# Phase 2: PostgreSQL/ODB Toolchain Baseline

## Purpose

Establish a reliable, documented PostgreSQL/ODB foundation gate. Verify ODB
toolchain availability, bring `common/database/pgsql` into the staged build, and
document the exact steps needed to enable ODB build/test.

## Separate Concerns

ODB has two independent components:

1. **ODB runtime libraries** (`libodb`, `libodb-pgsql`) — installed through
   vcpkg manifest feature `pgsql-odb`. These provide the C++ runtime headers
   and static libraries needed to compile and link ODB-using code.

2. **ODB compiler** (`odb` binary) — a separate code generator that produces
   `*-odb.hxx` / `*-odb.ixx` / `*-odb.cxx` mapping files from ODB-annotated
   headers. It is not distributed through vcpkg and must be installed manually.

## Toolchain Investigation

Commands run:

```bash
command -v odb
odb --version
```

Result: `odb` compiler is **NOT AVAILABLE**.

The ODB compiler is not distributed through vcpkg. It must be installed
manually from https://www.codesynthesis.com/odb/.

## Runtime Library Status

ODB runtime libraries are available through a vcpkg manifest feature:

| Package | Version | vcpkg port | Feature |
|---------|---------|------------|---------|
| `libodb` | 2.4.0 | `libodb` | `pgsql-odb` |
| `libodb-pgsql` | 2.4.0 | `libodb-pgsql` | `pgsql-odb` |

CMake targets: `odb::libodb`, `odb::libodb-pgsql`

The ODB runtime libraries are behind a **vcpkg manifest feature** so that
Gateway/Auth developers are not forced to install PostgreSQL/ODB dependencies.
To enable:

```bash
cmake -S . -B /tmp/mychat-build \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DCMAKE_BUILD_TYPE=Debug
```

With only `MYCHAT_BUILD_PGSQL_ODB=ON` and without
`-DVCPKG_MANIFEST_FEATURES=pgsql-odb`, CMake prints a clear warning directing
the user to the correct command.

## Build Infrastructure Changes

1. **`vcpkg.json`** — Added `features.pgsql-odb` containing `libodb` and
   `libodb-pgsql`. These are **not** in root `dependencies`.
2. **`CMakeLists.txt` (root)** — Added:
   - `MYCHAT_BUILD_PGSQL_ODB` option (default `OFF`)
   - `find_package(odb CONFIG COMPONENTS libodb pgsql)` when enabled
   - `MYCHAT_HAS_ODB_RUNTIME` variable set based on detection
   - Warning text guides users to `-DVCPKG_MANIFEST_FEATURES=pgsql-odb` when
     ODB is not found.
3. **`common/CMakeLists.txt`** — Added `im_pgsql` static library target:
   - Source: `database/pgsql/pgsql_conn.cpp`
   - Links: `odb::libodb`, `odb::libodb-pgsql`, `spdlog::spdlog`, `nlohmann_json::nlohmann_json`
4. **`common/database/pgsql/pgsql_conn.cpp`** — Fixed `auto` logger declaration
   to use explicit type matching the `extern` declaration in the header.

## Compile Fix

The file `pgsql_conn.cpp` declared:
```cpp
auto logger = LogManager::GetLogger("pgsql");
```

But the header declared:
```cpp
extern std::shared_ptr<spdlog::logger> logger;
```

`auto` created a new local variable instead of defining the `extern` declaration.
Fixed to:
```cpp
std::shared_ptr<spdlog::logger> logger = LogManager::GetLogger("pgsql");
```

## Source Review

| File | Assessment |
|------|------------|
| `pgsql_conn.hpp` | Well-structured RAII wrapper. Contains `extern spdlog::logger` declaration (now fixed via explicit type). |
| `pgsql_conn.cpp` | Full PgSqlConfig and PgSqlConnection implementation. Compiles and links. |
| `pgsql.conn.cpp` | Empty file (0 lines). Not referenced by any CMakeLists.txt. Leftover / placeholder. |
| `pgsql_mgr.hpp` | Header-only template-based connection pool manager. Depends on `ConnectionPool<T>` template. Not yet compiled — needs instantiation or .cpp. |
| `initDB.hpp` | Contains SQLite-specific `sqlite_master` query. Not production-ready. Not referenced by any CMakeLists.txt. |
| `services/odb/user.hpp` | ODB-annotated user entity. Issues: `#pragma db id auto` on `std::string uid_` (ODB does not support auto-generated string IDs). Missing `password_hash_` field. No namespace. |

## ODB Entity (`user.hpp`) Issues

Review of `services/odb/user.hpp`:

1. `#pragma db id auto` on `std::string uid_` — `auto` requires an integral type.
   ODB will reject this at ODB compilation time. Should be `#pragma db id` (manual
   string ID) or use an integral primary key.
2. Missing `password_hash_` field (needed for User Service MVP login).
3. No namespace — the class `user` is in the global namespace.
4. Some decorative fields (wxid, qqid, real_name, extra) may be reduced for MVP.

These issues should be addressed in a dedicated ODB model cleanup before User
Service MVP begins.

## Blocked By Toolchain

The `odb` compiler binary is required to:

1. Generate `*-odb.hxx`, `*-odb.ixx`, `*-odb.cxx` mapping files from ODB-annotated
   headers.
2. Compile those generated files into the build.
3. Run any ODB persistence test.

**Status: BLOCKED_BY_TOOLCHAIN** — No ODB persistence test can be added until
the `odb` compiler is installed.

### Installation Recommendation

Install the ODB compiler from https://www.codesynthesis.com/odb/odb.html :

```bash
# Ubuntu/Debian (if package available):
sudo apt install odb

# Or download and extract from:
# https://www.codesynthesis.com/download/odb/
```

After installation, verify:

```bash
odb --version
```

Then configure with ODB enabled:

```bash
cmake -S . -B /tmp/mychat-build \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-build -j2
ctest --test-dir /tmp/mychat-build -R 'PgSql|ODB|Postgres' --output-on-failure
```

## Verified Commands

### With ODB enabled (`MYCHAT_BUILD_PGSQL_ODB=ON` + `VCPKG_MANIFEST_FEATURES=pgsql-odb`):

```bash
cmake -S . -B /tmp/mychat-task2 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task2 -j2
ctest --test-dir /tmp/mychat-task2 -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Results:
- Configuration: ODB runtime libraries found. PostgreSQL/ODB support enabled.
- Build: All targets built successfully (im_pgsql included).
- Tests: 100% passed (2/2).

### Without ODB (default):

```bash
cmake -S . -B /tmp/mychat-task2-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task2-no-odb --target test_auth_tokens test_redis_hiredis gateway_server -j2
```

Results:
- Configuration: PostgreSQL/ODB targets are disabled.
- Build: Succeeded (im_pgsql skipped).
- Baseline not broken.

```bash
ctest --test-dir /tmp/mychat-task2-no-odb -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```
Result: 100% passed (2/2).

## Next Steps

1. Install the ODB compiler (`odb`) from the official website.
2. Fix `services/odb/user.hpp` issues (string `id auto`, missing password_hash,
   namespace).
3. Generate ODB mapping files from `user.hpp`.
4. Add focused ODB persistence test against Docker PostgreSQL.
5. Start User Service MVP implementation.

## Remaining Limitations

- No ODB persistence test can run without the `odb` compiler.
- `pgsql_mgr.hpp` (connection pool) is not yet compiled or tested.
- `initDB.hpp` contains SQLite-specific code and needs rewriting for PostgreSQL.
- `user.hpp` model needs cleanup before use.
