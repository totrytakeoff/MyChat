# Task 3 Review: ODB User Persistence Baseline

## Decision

`APPROVED`

This revision fixes the two blocking issues from the previous review:

1. ODB-enabled builds now fail at configure time unless a matching ODB 2.5.0
   runtime is available.
2. `ODBUserPersistenceTest` no longer drops the shared `im_users` table and
   now uses test-prefixed cleanup.

The baseline is reproducible on the current workstation and the no-ODB path
remains green.

## Findings

### Low: One generated-file regeneration hint still references a helper that does not exist

File:

- `services/odb/CMakeLists.txt`

The missing-file error message still mentions:

```text
odb -I$(scripts/find_vcpkg_include.sh) ...
```

That helper script is not present in the repo. The rest of the message is
usable because it also points to `scripts/build_odb_runtime_2_5.sh`, but the
regeneration hint should be cleaned up in a follow-up to avoid confusion.

This is not blocking approval because the actual runtime gate and verification
path are correct now.

## Reviewer Verification

ODB-enabled build/test on current workstation:

```bash
cmake -S . -B /tmp/mychat-review-odb \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-review-odb \
  --target im_pgsql im_user_odb test_odb_user_persistence \
          test_auth_tokens test_redis_hiredis -j2
ctest --test-dir /tmp/mychat-review-odb \
  -R 'ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' \
  --output-on-failure
```

Result: passed, 3/3 tests.

Default no-ODB baseline:

```bash
cmake -S . -B /tmp/mychat-review-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-review-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-review-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

Result: passed, 2/2 tests.

Fresh-checkout simulation without `.odb/`:

```bash
rsync -a --exclude='.git' --exclude='.odb' ./ \
  /tmp/mychat-review-no-local-odb-src/
cmake -S /tmp/mychat-review-no-local-odb-src \
  -B /tmp/mychat-review-no-local-odb-build \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
```

Result: configure failed immediately with a clear `ODB 2.5.0 runtime not found`
message. This is the desired behavior.

## Next Recommended Task

Proceed to the next Task 4 plan for User Service MVP groundwork.

