# Task 3 Review: ODB User Persistence Baseline

## Decision

`CHANGES_REQUESTED`

The implemented User ODB model and persistence test pass on the current
workstation when the local `.odb/installed` runtime exists. That proves the
basic ODB 2.5.0 persistence path is viable. However, the implementation is not
yet reproducible from a normal checkout, and CMake currently accepts a known-bad
fallback to vcpkg ODB 2.4.0 even though the generated files require ODB 2.5.0.

This must be fixed before Task 3 can be approved.

## Findings

### High: ODB-enabled build is not reproducible without untracked `.odb/installed`

Files:

- `CMakeLists.txt`
- `services/odb/generated/user-odb.hxx`
- `docs/devlog/phase3_odb_user_persistence.md`

The generated header contains:

```cpp
#if ODB_VERSION != 20500UL
#error ODB runtime version mismatch
#endif
```

But `CMakeLists.txt` falls back to the vcpkg ODB 2.4.0 package if
`.odb/installed/lib/libodb.a` and `.odb/installed/lib/libodb-pgsql.a` do not
exist. That fallback configures successfully, then fails during compilation.

Reviewer verification from a temporary source copy with `.odb/` excluded:

```bash
rsync -a --exclude='.git' --exclude='.odb' ./ \
  /tmp/mychat-task3-review-no-local-odb-src/
cmake -S /tmp/mychat-task3-review-no-local-odb-src \
  -B /tmp/mychat-task3-review-no-local-odb-build \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-review-no-local-odb-build \
  --target im_user_odb -j2
```

Result: configure reported vcpkg ODB 2.4.0 as found, then build failed with
`#error ODB runtime version mismatch` and additional 2.4/2.5 API mismatch
errors.

Required fix:

- Do not silently fall back to vcpkg ODB 2.4.0 for generated 2.5.0 artifacts.
- When generated files require `ODB_VERSION == 20500`, CMake should either:
  - find a real ODB 2.5.0 runtime through a documented explicit prefix such as
    `MYCHAT_ODB_ROOT`; or
  - fail at configure time with a clear error that tells the developer how to
    install/build the 2.5.0 runtime.
- Do not make `.odb/installed` an implicit, untracked requirement for a
  supposedly completed baseline.

Acceptable directions:

- Add a tracked script, e.g. `scripts/build_odb_runtime_2_5.sh`, that downloads
  or builds the 2.5.0 runtime into `.odb/installed`, then make CMake fail
  clearly if it is absent.
- Or require a system/prefix install and expose `-DMYCHAT_ODB_ROOT=/path`.
- Or regenerate with an ODB compiler/runtime pair that is actually provided by
  vcpkg. Given the installed compiler is 2.5.0, this likely means using ODB
  runtime 2.5.0 explicitly.

### Medium: `.odb/` source/runtime artifacts are untracked but documentation depends on them

Files:

- `.odb/`
- `docs/devlog/phase3_odb_user_persistence.md`

The summary says `.odb/installed/` is "not in version control", yet the docs
refer to `.odb/src/libodb-2.5.0` and `.odb/src/libodb-pgsql-2.5.0` as if they
exist. A clean checkout will not have those files.

Required fix:

- Either commit a small reproducible setup script and document it as the source
  of truth, or move the dependency outside the repo and require an explicit
  install prefix.
- Do not document manual steps that depend on untracked local source trees
  unless the exact download/build steps are included.

### Medium: Persistence test drops the shared `im_users` table

File: `test/odb/test_odb_user_persistence.cpp`

`SetupTable()` runs:

```cpp
DROP TABLE IF EXISTS "im_users" CASCADE
```

This is acceptable for a throwaway isolated test database, but the current test
uses the shared development database `mychat` from `config/dev.json`. As soon as
User Service development starts, this can erase real local dev data or break
concurrent tests.

Required fix:

- Use an isolated table/schema/database for the test, or make setup use the
  generated schema in a controlled disposable database.
- At minimum, the test should not drop the production-named `im_users` table in
  the shared `mychat` database.

### Low: Architecture docs overstate completion

Files:

- `docs/architecture/service_mvp_roadmap.md`
- `docs/devlog/current_progress.md`

The roadmap marks Phase C as complete, but the baseline still depends on an
untracked local ODB runtime. The code path is promising, but this is not yet a
portable project baseline.

Required fix:

- Downgrade status wording until the ODB 2.5.0 runtime setup is reproducible and
  CMake fails early on incompatible runtimes.
- After fixing dependency setup and test isolation, Phase C can be marked
  complete.

## Reviewer Verification

Docker dependencies:

```bash
docker compose up -d redis postgres
docker compose exec -T postgres pg_isready -U mychat -d mychat
docker compose exec -T redis redis-cli -a mychat-dev-pass ping
```

Result: PostgreSQL ready, Redis returned `PONG`.

ODB-enabled build/test on current workstation:

```bash
cmake -S . -B /tmp/mychat-task3-review-odb \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-review-odb \
  --target im_pgsql im_user_odb test_odb_user_persistence \
           test_auth_tokens test_redis_hiredis -j2
ctest --test-dir /tmp/mychat-task3-review-odb \
  -R 'ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' \
  --output-on-failure
```

Result: passed, 3/3 tests.

Default no-ODB baseline:

```bash
cmake -S . -B /tmp/mychat-task3-review-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-review-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task3-review-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result: passed, 2/2 tests.

Fresh-checkout simulation without `.odb/`:

```bash
rsync -a --exclude='.git' --exclude='.odb' ./ \
  /tmp/mychat-task3-review-no-local-odb-src/
cmake -S /tmp/mychat-task3-review-no-local-odb-src \
  -B /tmp/mychat-task3-review-no-local-odb-build \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task3-review-no-local-odb-build \
  --target im_user_odb -j2
```

Result: configure passed incorrectly; build failed with ODB runtime mismatch.

## Required Revision

Please revise Task 3 before moving on:

1. Make ODB 2.5.0 runtime setup reproducible or explicitly required through a
   stable CMake variable.
2. Remove the silent vcpkg 2.4.0 fallback for generated 2.5.0 ODB files.
3. Make CMake fail at configure time when the available runtime does not match
   the generated ODB version.
4. Isolate `ODBUserPersistenceTest` so it does not drop the shared `im_users`
   table in the development database.
5. Update docs and `task3-summary.md` with the revision details and exact
   verification commands.

Do not start User Service business logic in this revision.
