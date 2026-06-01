# Task 3 Summary: ODB User Persistence Baseline

## Files Created

- `services/odb/CMakeLists.txt` — Defines `im_user_odb` static library target
  (alias `im::user_odb`), gated by `MYCHAT_BUILD_PGSQL_ODB=ON` + ODB runtime
  found. Fails with regeneration instructions if generated files are missing.
- `services/odb/generated/user-odb.hxx` — ODB-generated header (persistence
  traits, session, queries for `im::service::user::User`).
- `services/odb/generated/user-odb.ixx` — ODB-generated inline definitions.
- `services/odb/generated/user-odb.cxx` — ODB-generated implementation
  (database operations for PostgreSQL).
- `services/odb/generated/user.sql` — ODB-generated DDL (CREATE TABLE, indexes).
- `test/odb/CMakeLists.txt` — Defines `test_odb_user_persistence` test target,
  gated by `im::user_odb` availability.
- `test/odb/test_odb_user_persistence.cpp` — `ODBUserPersistenceTest` with 2
  test cases (persist and load, determinism/repeatability).
- `docs/devlog/phase3_odb_user_persistence.md` — Full investigation log: model
  design, ODB generation, source-buil t runtime, test implementation, results.
- `.odb/installed/` — Locally built ODB 2.5.0 runtime (libodb.a, libodb-pgsql.a,
  headers). Not in version control.

## Files Changed

- `services/odb/user.hpp` — Complete rewrite: namespace `im::service::user`,
  class `User`, table `im_users`, manual string UID, added `password_hash_`,
  `friend class odb::access`.
- `CMakeLists.txt` (root) — Added ODB detection logic that prefers local
  `.odb/installed/` (2.5.0) over `find_package(odb CONFIG)` (vcpkg 2.4.0).
  Uses `PostgreSQL::PostgreSQL` imported target to fix generator-expression
  dependency chain. Includes `services/odb/` subdirectory behind the ODB gate.
- `common/CMakeLists.txt` — Changed `im_pgsql` link from `PostgreSQL_LIBRARIES`
  (bare string) to `PostgreSQL::PostgreSQL` (imported target) for correct
  INTERFACE_LINK_LIBRARIES propagation.
- `docs/devlog/current_progress.md` — Moved ODB user persistence to completed
  work; updated next tasks and risks.
- `docs/architecture/mvp_architecture.md` — Removed stale "PostgreSQL/ODB not
  validated" claim; updated technical debt.
- `docs/architecture/service_mvp_roadmap.md` — Phase C status changed to
  "complete" with actual scope and exit criteria.

## Behavior Changed

- `MYCHAT_BUILD_PGSQL_ODB=ON` now additionally:
  - Detects ODB runtime (local `.odb/installed/` 2.5.0 preferred; vcpkg 2.4.0
    fallback).
  - Builds the `im_user_odb` static library from generated ODB sources.
  - Builds and runs `ODBUserPersistenceTest` against Docker PostgreSQL.
- `MYCHAT_BUILD_PGSQL_ODB=OFF` (default): unchanged. All ODB targets are
  skipped. The existing Gateway/Auth baseline is preserved.
- `.odb/installed/` local build replaces the plan's assumption that vcpkg 2.4.0
  runtime would work with the 2.5.0 compiler (it did not: 2.4.0 headers lack
  `inline` constexpr support required by 2.5.0-generated code).

## Verification

### ODB Generation

```bash
odb --version
# ODB object-relational mapping (ORM) compiler for C++ 2.5.0

mkdir -p services/odb/generated
odb --database pgsql --std c++20 --generate-query --generate-schema \
  --schema-format sql --output-dir services/odb/generated \
  --hxx-suffix .hxx --ixx-suffix .ixx --cxx-suffix .cxx --sql-suffix .sql \
  services/odb/user.hpp
# Produced: user-odb.hxx, user-odb.ixx, user-odb.cxx, user.sql
```

### ODB-enabled build and test (all 3 tests pass)

```bash
cmake -S . -B /tmp/mychat-task3-odb \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-odb \
  --target im_pgsql im_user_odb test_odb_user_persistence \
          test_auth_tokens test_redis_hiredis -j2
ctest --test-dir /tmp/mychat-task3-odb \
  -R 'ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' \
  --output-on-failure
# Result: 3/3 tests passed
```

### Default no-ODB baseline

```bash
cmake -S . -B /tmp/mychat-task3-no-odb \
  -DMYCHAT_BUILD_TESTS=ON -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task3-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
# Result: 2/2 tests passed, baseline green
```

## Deviations From Plan

1. **ODB runtime 2.5.0 source build**: The plan assumed vcpkg 2.4.0 runtime
   might work with the 2.5.0 compiler. It did not — 2.4.0 generated code calls
   `odb::container_traits_impl::dynamically_initialized` which is `constexpr`
   in 2.5.0 headers but not in 2.4.0. Built both `libodb` and `libodb-pgsql`
   from source.

2. **`odb::pgsql::database` direct usage instead of `PgSqlConnection`**: The
   plan suggested using the existing RAII wrapper. The wrapper has pre-existing
   bugs (`std::to_string` for string IDs, `shared_ptr` vs raw-pointer return)
   that were not introduced by this task. Test uses the ODB database object
   directly to avoid blocking the baseline.

3. **Schema setup via direct SQL instead of `user.sql`**: The plan preferred
   using the generated `user.sql` file. The test uses `CREATE TABLE IF NOT EXISTS
   im_users (...)` and `DROP TABLE IF EXISTS im_users` directly for
   determinism — cleaner than parsing a generated schema file that could
   include type-specific PostgreSQL clauses.

4. **`PostgreSQL::PostgreSQL` imported target**: The plan specified
   `PostgreSQL_LIBRARIES` (bare string). At link time, this missed the OpenSSL
   dependency chain from libpq. Changed to the imported target which correctly
   propagates `INTERFACE_LINK_LIBRARIES`.

## Known Issues / Follow-Up

- **ODB runtime source-build dependency**: `.odb/installed/` is not in version
  control. Any developer checking out fresh must run the documented build
  steps (or wait for vcpkg 2.5.0 packages).
- **`pgsql_conn.hpp` RAII wrapper issues**:
  - `std::to_string(t->id())` fails for string UID types (needs
    `t->uid()` or specialization).
  - `session->load<T>(id)` returns `std::shared_ptr<T>` but the caller
    receives raw pointer (implicit conversion may be incorrect).
  - `erase()` passes raw pointer where shared_ptr is expected.
- **No schema migration framework**: The test creates/drops `im_users` directly.
  Production User Service needs a proper bootstrap or migration strategy.
- **Password hashing not implemented**: `password_hash_` exists in the model
  but no hashing algorithm has been chosen or implemented.

## Acceptance Criteria (All Met)

- [x] User model is valid, namespaced (`im::service::user::User`), includes
      `password_hash`.
- [x] ODB generated files present and reproducible with documented command.
- [x] CMake builds `im_user_odb` behind the ODB gate.
- [x] `ODBUserPersistenceTest` passes against Docker PostgreSQL.
- [x] Existing Auth/Redis tests still pass.
- [x] Default no-ODB baseline still passes.
- [x] Docs accurately reflect new ODB toolchain and persistence status.
- [x] `docs/agent_context/task3-summary.md` written.

---

## Revision 1 (2026-06-01) — Reproducible ODB runtime & CMake version gating

### Changes

1. **`scripts/build_odb_runtime_2_5.sh`** — New tracked build script that
   downloads, builds, and installs both `libodb` and `libodb-pgsql` 2.5.0
   from source. Accepts `--prefix`, `--src-dir`, `--pg-include-dir`, `--clean`.

2. **`CMakeLists.txt` (root)** — Replaced the silent fallback from `.odb/installed/`
   → vcpkg 2.4.0 with a strict 2.5.0 requirement:
   - Adds `MYCHAT_ODB_ROOT` cache variable (explicit prefix override).
   - Searches `MYCHAT_ODB_ROOT` first, then `.odb/installed/`.
   - Reads `version.hxx` to verify `ODB_VERSION == 20500` at configure time.
   - If generated ODB files exist and no 2.5.0 runtime is found, **fails at
     configure time** with instructions to run `scripts/build_odb_runtime_2_5.sh`.
   - Removed the `find_package(odb CONFIG)` vcpkg 2.4.0 path entirely.

3. **`test/odb/test_odb_user_persistence.cpp`** — `SetupTable` no longer drops
   the shared `im_users` table. Uses `CREATE TABLE IF NOT EXISTS` and
   `DELETE FROM "im_users" WHERE "uid" LIKE 'test-%'` for clean isolation.

4. **`services/odb/CMakeLists.txt`** — Updated missing-file error to reference
   `scripts/build_odb_runtime_2_5.sh`.

5. **`docs/devlog/phase3_odb_user_persistence.md`** — Build instructions now
   reference the tracked script instead of manual steps.

6. **`docs/devlog/current_progress.md`** — Updated to reflect reproducible
   baseline and CMake-time version enforcement.

7. **`docs/architecture/service_mvp_roadmap.md`** — Phase C exit criteria
   updated to include the build script and CMake version gating.

### Deviations From Plan (Revision)

- The original implementation's implicit `.odb/installed/` dependency and
  vcpkg 2.4.0 fallback were replaced by an explicit, tracked build script
  and strict CMake version enforcement, as required by the review.

### Verification

Clean checkout simulation — ODB-off baseline:

```bash
rsync -a --exclude='.git' --exclude='.odb' ./ /tmp/mychat-task3-rev-no-odb-src/
cmake -S /tmp/mychat-task3-rev-no-odb-src \
  -B /tmp/mychat-task3-rev-no-odb-build \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-rev-no-odb-build \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task3-rev-no-odb-build \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
# Expected: 2/2 passed
```

Clean checkout simulation — ODB-enabled should FAIL at configure time:

```bash
rsync -a --exclude='.git' --exclude='.odb' ./ /tmp/mychat-task3-rev-no-local-src/
cmake -S /tmp/mychat-task3-rev-no-local-src \
  -B /tmp/mychat-task3-rev-no-local-build \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
# Expected: FATAL_ERROR with instructions to run build_odb_runtime_2_5.sh
```

ODB-enabled build/test with runtime present:

```bash
scripts/build_odb_runtime_2_5.sh
cmake -S . -B /tmp/mychat-task3-rev-odb \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-rev-odb \
  --target im_pgsql im_user_odb test_odb_user_persistence \
          test_auth_tokens test_redis_hiredis -j2
ctest --test-dir /tmp/mychat-task3-rev-odb \
  -R 'ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' \
  --output-on-failure
# Expected: 3/3 passed
```
