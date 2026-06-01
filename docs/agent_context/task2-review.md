# Task 2 Review: PostgreSQL/ODB Toolchain Baseline

## Decision

`CHANGES_REQUESTED`

The implementation successfully proves that `im_pgsql` can compile when ODB
runtime libraries are present, and it preserves the normal CMake target graph
when `MYCHAT_BUILD_PGSQL_ODB=OFF`. However, the vcpkg manifest now installs ODB
runtime libraries unconditionally, which violates the task requirement that
Gateway/Auth developers must not need ODB just to configure the baseline.

## Findings

### High: ODB runtime is now an unconditional vcpkg dependency

File: `vcpkg.json`

`libodb` and `libodb-pgsql` were added directly to the root `dependencies`
array. In vcpkg manifest mode, those packages are installed during configure
even when CMake is run with the default `MYCHAT_BUILD_PGSQL_ODB=OFF`.

Reviewer verification:

```bash
cmake -S . -B /tmp/mychat-task2-review-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
```

Result: configure succeeded and printed that PostgreSQL/ODB targets were
disabled, but vcpkg still installed/resolved `libodb`, `libodb-pgsql`, and their
PostgreSQL dependencies because they are root dependencies.

Required fix:

- Move `libodb` and `libodb-pgsql` into a manifest feature, for example:

```json
"features": {
  "pgsql-odb": {
    "description": "PostgreSQL ODB runtime libraries.",
    "dependencies": [
      "libodb",
      "libodb-pgsql"
    ]
  }
}
```

- Keep the default dependency set free of ODB runtime libraries.
- Document the enable command using the CMake toolchain variable that vcpkg
  already supports:

```bash
cmake -S . -B /tmp/mychat-task2 \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  ...
```

`/home/myself/pkgs/vcpkg/scripts/buildsystems/vcpkg.cmake` maps
`VCPKG_MANIFEST_FEATURES` entries to `vcpkg install --x-feature=<feature>`.

### Medium: ODB compiler status must stay distinct from ODB runtime status

Files: `CMakeLists.txt`, `docs/devlog/phase2_odb_toolchain.md`

The current docs correctly note that the `odb` compiler is missing, but the CMake
option text says `MYCHAT_BUILD_PGSQL_ODB` requires compiler and runtime while
the implemented target only requires runtime libraries. This can confuse the
next task boundary.

Required fix:

- Make terminology consistent:
  - `MYCHAT_BUILD_PGSQL_ODB` currently enables runtime-backed `im_pgsql`.
  - ODB entity generation/persistence tests remain blocked until the `odb`
    compiler binary is installed.
- In docs, explicitly say vcpkg provides runtime libraries only, not the ODB
  compiler.

### Low: Redis/Auth integration tests are not isolated across parallel build directories

Files: `test/db/test_redis_hiredis.cpp`, `test/auth/test_auth_tokens.cpp`

Both tests use Redis DB 15 and fixed key names. Running the same tests in two
build directories concurrently causes cross-test deletion and transient
failures. Sequential runs pass.

This is not required to finish Task 2, but record it as follow-up for the test
infrastructure phase. Before CI parallelization, tests should use per-process or
per-build key prefixes, or isolated Redis DBs.

## Reviewer Verification

Docker dependencies:

```bash
docker compose up -d redis postgres
```

ODB compiler:

```bash
command -v odb
odb --version
```

Result: `odb` command not found.

Default no-ODB build:

```bash
cmake --build /tmp/mychat-task2-review-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
```

Result: passed.

ODB runtime build:

```bash
cmake --build /tmp/mychat-task2-review-odb -j2
```

Result: passed. `im_pgsql` built successfully.

Sequential baseline tests:

```bash
ctest --test-dir /tmp/mychat-task2-review-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
ctest --test-dir /tmp/mychat-task2-review-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result: both passed, 2/2 tests each.

Parallel test note:

Running the two CTest commands concurrently caused Redis key collisions and
failures in `RedisHiredisTest` / `AuthTokenTest`. Sequential rerun passed, so
this is recorded as a test isolation risk rather than a Task 2 code regression.

## Required Revision

Please revise Task 2 before we move to Task 3:

1. Move ODB runtime packages from root `dependencies` into a vcpkg feature.
2. Update `docs/devlog/phase2_odb_toolchain.md` and
   `docs/devlog/current_progress.md` with the feature-based enable command.
3. Adjust wording so ODB runtime and ODB compiler are clearly separate.
4. Append a revision section to `docs/agent_context/task2-summary.md` with the
   exact changes and verification results.

Do not add User Service business logic in this revision.
