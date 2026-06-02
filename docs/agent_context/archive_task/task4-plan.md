# Task 4 Plan: User Service Core MVP Baseline

## Status

Ready for implementation.

## Owner

Implementation Agent.

## Reviewer

Planner/Reviewer Agent.

## Context

Task 3 completed the PostgreSQL/ODB foundation:

- `services/odb/user.hpp` is now a valid ODB entity model.
- ODB 2.5.0 generated files are committed under `services/odb/generated/`.
- `im_user_odb` builds behind `MYCHAT_BUILD_PGSQL_ODB=ON`.
- `ODBUserPersistenceTest` passes against Docker PostgreSQL.
- ODB runtime 2.5.0 is built through `scripts/build_odb_runtime_2_5.sh`.

The project direction remains microservice-first. MVP does not mean collapsing
Gateway/User/Message into one process. The next vertical slice should make User
Service a real independently buildable and testable module, while still keeping
Gateway integration for a later task.

Current relevant state:

- `services/user/user_service.hpp` is only an empty skeleton.
- `services/CMakeLists.txt` only creates `im_user_service` if `user/*.cpp`
  exists.
- `common/proto/user.proto` contains broad user messages, but gRPC/service
  artifacts are not currently a reliable build baseline.
- `common/database/pgsql/PgSqlConnection` has known template issues for string
  IDs and pointer ownership. Do not make User Service depend on that wrapper in
  this task unless you fix it with focused tests.

## Goal

Create the first real User Service core MVP baseline:

1. A `services/user` CMake target exists and builds with ODB enabled.
2. User Service can register an account, reject duplicates, login with a
   password, fetch profile by uid/account, and update `last_login`.
3. Passwords are hashed before persistence; plaintext passwords are never
   stored in `im_users`.
4. Focused tests under `test/user` pass against Docker PostgreSQL.
5. Docs record the service boundary, implemented behavior, and verification
   commands.

This is a service-core task, not Gateway integration.

## Relevant Files

Read before editing:

- `docs/architecture/mvp_architecture.md`
- `docs/architecture/service_mvp_roadmap.md`
- `docs/devlog/current_progress.md`
- `docs/devlog/phase3_odb_user_persistence.md`
- `services/odb/user.hpp`
- `services/odb/generated/user-odb.hxx`
- `services/user/user_service.hpp`
- `services/CMakeLists.txt`
- `test/CMakeLists.txt`
- `common/proto/user.proto`
- `common/database/pgsql/pgsql_conn.hpp`

Likely files to create/update:

- `services/user/CMakeLists.txt`
- `services/user/user_service.hpp`
- `services/user/user_service.cpp`
- `services/user/user_repository.hpp`
- `services/user/user_repository.cpp`
- `services/user/password_hasher.hpp`
- `services/user/password_hasher.cpp`
- `test/user/CMakeLists.txt`
- `test/user/test_user_service.cpp`
- `docs/devlog/phase4_user_service_core.md`
- `docs/devlog/current_progress.md`
- `docs/architecture/service_mvp_roadmap.md`
- `docs/agent_context/task4-summary.md`

## Required Design

### Namespaces

Use:

```cpp
namespace im::service::user
```

Avoid adding more global classes.

### User Repository

Create a small ODB-backed repository around `im::service::user::User`.

Suggested shape:

```cpp
class UserRepository {
public:
    explicit UserRepository(std::shared_ptr<odb::pgsql::database> db);

    bool create(const User& user);
    std::optional<User> find_by_uid(const std::string& uid);
    std::optional<User> find_by_account(const std::string& account);
    bool account_exists(const std::string& account);
    bool update_last_login(const std::string& uid, int64_t last_login);
};
```

Notes:

- It is acceptable to use `odb::pgsql::database` directly in this task.
- Do not depend on `PgSqlConnection` unless you repair its known string-ID and
  pointer ownership bugs with focused tests.
- Keep SQL cleanup/setup in tests narrow and deterministic.
- Handle duplicate account creation explicitly. ODB/PostgreSQL unique constraint
  failures should become a service-level result, not a raw exception escaping
  normal duplicate registration.

### Password Hasher

Implement a small password hashing helper using already available dependencies.

Preferred direction:

- Use OpenSSL PBKDF2-HMAC-SHA256.
- Store hashes in a self-describing format such as:

```text
pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>
```

Minimum requirements:

- Generate a random salt for new passwords.
- Use a configurable/default iteration count. Choose a conservative development
  value that keeps tests fast, and document it.
- Use constant-time comparison where practical for verification.
- Never store plaintext passwords.
- Tests must prove the stored `password_hash` differs from the original
  password.

Do not add a new crypto dependency unless OpenSSL cannot satisfy the task.

### User Service Core

Create a business-level service class that wraps repository and password hashing.

Suggested DTO/result style:

```cpp
struct ServiceResult {
    bool ok;
    std::string error_code;
    std::string message;
};

struct UserProfile {
    std::string uid;
    std::string account;
    std::string nickname;
    std::string avatar;
    Gender gender;
    std::string signature;
    int64_t create_time;
    int64_t last_login;
    bool online;
};
```

Possible service operations:

```cpp
RegisterResult register_user(const RegisterRequest& request);
LoginResult login_by_account(const std::string& account,
                             const std::string& password,
                             int64_t now_ms);
std::optional<UserProfile> get_profile_by_uid(const std::string& uid);
std::optional<UserProfile> get_profile_by_account(const std::string& account);
```

You may define lightweight C++ request/result structs instead of using protobuf
messages directly. The important thing in this task is stable service-core
behavior. Gateway/protobuf/gRPC wiring is explicitly out of scope.

UID generation may use `uuid`/libuuid if already available, or a small helper
based on existing project utilities. Do not use database auto IDs.

## Required Behavior

Register:

- Reject empty account.
- Reject empty password.
- Reject duplicate account.
- Create `User` with:
  - manual string UID;
  - account;
  - password hash;
  - nickname defaulting to account if not supplied;
  - create_time and last_login set from input/current time;
  - online false by default.
- Return a profile that does not expose `password_hash`.

Login:

- Reject unknown account.
- Reject wrong password.
- Accept correct password.
- Update `last_login` on successful login.
- Return a profile that does not expose `password_hash`.

Profile:

- Fetch profile by UID.
- Fetch profile by account.
- Return no profile for missing users.

## Required Build Changes

Update service build integration so `im_user_service` is deterministic.

Avoid relying only on `file(GLOB_RECURSE user/*.cpp)` for the new service if
you can keep the source list explicit and readable.

Expected target behavior:

- `im_user_service` builds when:
  - `MYCHAT_BUILD_SERVICES=ON`
  - `MYCHAT_BUILD_PGSQL_ODB=ON`
  - `im::user_odb` is available
- It links:
  - `im::user_odb`
  - `im::utils`
  - `OpenSSL::Crypto`
  - `unofficial::UUID::uuid` if UUID generation uses libuuid
- If ODB is not enabled, User Service should be skipped with a clear CMake
  status message, not partially built.

Add `test/user` only when `im::user_service` exists.

## Required Tests

Add focused tests under:

```text
test/user/
```

Test target name:

```text
test_user_service
```

CTest name:

```text
UserServiceCoreTest
```

Tests should connect to Docker PostgreSQL using:

- host `127.0.0.1`
- port `5432`
- database `mychat`
- user `mychat`
- password `mychat-dev-pass`

Required test cases:

1. Register creates a user and stores a password hash, not plaintext.
2. Duplicate account registration is rejected.
3. Login succeeds with correct password and updates `last_login`.
4. Login fails with wrong password.
5. Profile lookup works by UID and account.

Test data isolation:

- Do not `DROP TABLE im_users`.
- Use `CREATE TABLE IF NOT EXISTS` only if needed.
- Clean test rows by a unique test account prefix, for example
  `task4-test-%`.
- Tests must be deterministic when re-run.

## Documentation Requirements

Create:

- `docs/devlog/phase4_user_service_core.md`

Update:

- `docs/devlog/current_progress.md`
- `docs/architecture/service_mvp_roadmap.md`
- `docs/agent_context/task4-summary.md`

Docs must include:

- files changed;
- service-core API surface;
- password hashing format and iteration count;
- PostgreSQL/ODB setup assumptions;
- exact CMake build/test commands;
- test results;
- known limitations before Gateway integration.

Also fix stale architecture wording if encountered, especially claims that
refresh-token Redis hardening is still pending.

## Required Verification

Start dependencies:

```bash
docker compose up -d redis postgres
docker compose exec -T postgres pg_isready -U mychat -d mychat
docker compose exec -T redis redis-cli -a mychat-dev-pass ping
```

ODB/User-service build:

```bash
cmake -S . -B /tmp/mychat-task4-user \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task4-user \
  --target im_user_service test_user_service test_auth_tokens \
          test_redis_hiredis test_odb_user_persistence -j2
ctest --test-dir /tmp/mychat-task4-user \
  -R 'UserServiceCoreTest|ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' \
  --output-on-failure
```

Default no-ODB baseline:

```bash
cmake -S . -B /tmp/mychat-task4-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task4-no-odb \
  --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task4-no-odb \
  -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

If verification fails, record the exact failure in `task4-summary.md` and do
not mark acceptance criteria as met.

## Out Of Scope

- Gateway HTTP route integration.
- JWT/refresh-token issuance changes.
- gRPC service generation or codec service cleanup.
- Message/Friend/Group/Push services.
- Full database migration framework.
- Production password policy, rate limiting, captcha, or verification-code
  workflow.
- Rewriting all of `common/database/pgsql`.

## Acceptance Criteria

Task 4 is ready for review when:

- `im_user_service` builds as a real target.
- User registration/login/profile core behavior is implemented.
- Password hashing is implemented and tested.
- User Service tests pass against Docker PostgreSQL.
- Existing ODB/Auth/Redis focused tests still pass.
- Default no-ODB baseline still passes.
- No test drops the shared `im_users` table.
- Docs and `task4-summary.md` are updated with exact verification results.

