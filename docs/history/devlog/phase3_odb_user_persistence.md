# Phase 3: ODB User Persistence Baseline

## Purpose

Create the first real PostgreSQL/ODB persistence baseline for the User domain.
Generate ODB compiled files from a valid user entity model, build them through
CMake, and run a focused persistence test against Docker PostgreSQL.

## Toolchain

| Component | Version | Source |
|-----------|---------|--------|
| `odb` compiler | 2.5.0 | `/usr/bin/odb` (system install) |
| `libodb` (runtime) | 2.5.0 | Built from source via `scripts/build_odb_runtime_2_5.sh` |
| `libodb-pgsql` (runtime) | 2.5.0 | Built from source via `scripts/build_odb_runtime_2_5.sh` |
| PostgreSQL | 16.9 (via libpq) | vcpkg feature `pgsql-odb` |

The ODB compiler (2.5.0) was pre-installed at `/usr/bin/odb`. The vcpkg
feature provides ODB runtime 2.4.0 libraries, which are incompatible with the
2.5.0 compiler (both header API and link-time symbols differ). The 2.5.0
runtime must be built from source using the tracked build script.

### Building ODB 2.5.0 Runtime From Source

The reproducible build script handles downloading, building, and installing
both `libodb` and `libodb-pgsql`:

```bash
scripts/build_odb_runtime_2_5.sh
```

This installs the runtime to `.odb/installed/` by default. For a custom prefix:

```bash
scripts/build_odb_runtime_2_5.sh --prefix /opt/odb-2.5.0
```

Then configure CMake with `-DMYCHAT_ODB_ROOT=/opt/odb-2.5.0`.

Prerequisites:
- build2 toolchain (`bdep`, `b`) — https://build2.org/
- g++ with C++20 support
- curl, tar (for downloading/extracting source tarballs)
- libpq headers (from vcpkg feature `pgsql-odb` or system package)

CMake detects the runtime at `.odb/installed/` by default. If the runtime
is not found and generated ODB files exist, CMake fails at configure time
with instructions. There is no silent fallback to vcpkg 2.4.0.

## User Entity Model

File: `services/odb/user.hpp`

- Namespace: `im::service::user`
- Entity class: `User`
- Table: `im_users`
- Primary key: manual string UID (UUID-based, not auto-generated)
- Fields: uid, account, password_hash, nickname, avatar, gender, signature,
  create_time, last_login, online
- `password_hash` - only the hash is stored, never plaintext
- `friend class odb::access;` for private member access

## ODB Generation

Command run from project root:

```bash
VCPKG_INC="/home/myself/pkgs/vcpkg/installed/x64-linux/include"
odb -I"$VCPKG_INC" \
    --database pgsql \
    --std c++20 \
    --generate-query \
    --generate-schema \
    --schema-format sql \
    --output-dir services/odb/generated \
    --hxx-suffix .hxx \
    --ixx-suffix .ixx \
    --cxx-suffix .cxx \
    --sql-suffix .sql \
    services/odb/user.hpp
```

Generated files (do not edit manually):

```
services/odb/generated/user-odb.hxx
services/odb/generated/user-odb.ixx
services/odb/generated/user-odb.cxx
services/odb/generated/user.sql
```

## Build Integration

Target: `im_user_odb` (alias `im::user_odb`)

- Type: static library
- Source: `services/odb/generated/user-odb.cxx`
- Include paths: `services/odb`, `services/odb/generated`
- Links: `odb::libodb`, `odb::libodb-pgsql`
- Gate: `MYCHAT_BUILD_PGSQL_ODB=ON` AND ODB runtime found

If generated files are missing when the gate is on, CMake fails with a
clear message telling the developer to run the `odb` command above.

Test target: `test_odb_user_persistence`

- Gate: `im::user_odb` target exists
- Links: `im::user_odb`, GTest, Threads
- Test name: `ODBUserPersistenceTest`

## Tests

### ODB-enabled

```bash
cmake -S . -B /tmp/mychat-task3 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3 \
  --target im_pgsql im_user_odb test_odb_user_persistence \
          test_auth_tokens test_redis_hiredis -j2
ctest --test-dir /tmp/mychat-task3 \
  -R 'ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' \
  --output-on-failure
```

Result: 3/3 passed (RedisHiredisTest: 1.2s, ODBUserPersistenceTest: 0.13s,
AuthTokenTest: 4.3s).

### Default no-ODB

```bash
cmake -S . -B /tmp/mychat-task3-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task3-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result: 2/2 passed. Baseline not broken.

## Remaining Boundary Before User Service MVP

1. **ODB compiler is a manual setup step**. The `odb` binary must be installed
   separately. The 2.5.0 runtime is built from source via
   `scripts/build_odb_runtime_2_5.sh` (until vcpkg packages are updated to
   2.5.0). CMake will fail at configure time if the runtime is missing.
2. **`services/odb/user.hpp`** - the User model is valid and tested, but
   register/login service logic is not implemented.
3. **Password hashing** - the test uses a hardcoded SHA-256 hash. A proper
   hashing algorithm (e.g., bcrypt, Argon2) should be chosen before
   production User Service MVP.
4. **`pgsql_conn.hpp` / `pgsql_conn.cpp`** - the RAII wrapper has pre-existing
   issues (`std::to_string` on string IDs, raw-pointer vs shared_ptr return).
   Not blocking ODB persistence baseline, but should be fixed before the
   User Service implementation depends on it.
5. **Database migrations** - no schema migration framework yet.
