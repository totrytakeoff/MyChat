# Phase 4: User Service Core MVP

## Goal

Create a real, testable User Service core with register/login/profile workflows
backed by ODB/PostgreSQL and password hashing via OpenSSL PBKDF2-SHA256.

This phase does not wire the service into Gateway HTTP routes. Gateway
integration is Phase E.

## Files Changed

### Created

- `services/user/password_hasher.hpp` — PBKDF2-SHA256 hashing header
- `services/user/password_hasher.cpp` — implementation (OpenSSL PKCS5_PBKDF2_HMAC)
- `services/user/user_repository.hpp` — ODB-backed repository header
- `services/user/user_repository.cpp` — CRUD: create, find_by_uid/account, update_last_login
- `services/user/user_service.hpp` — business service class with DTO structs
- `services/user/user_service.cpp` — register/login/profile logic
- `services/user/CMakeLists.txt` — `im_user_service` static library target
- `test/user/CMakeLists.txt` — `test_user_service` test target
- `test/user/test_user_service.cpp` — 5 test cases covering all required behaviors

### Modified

- `services/CMakeLists.txt` — replaced GLOB-based user service with
  `add_subdirectory(user)` gated on `TARGET im::user_odb`
- `test/CMakeLists.txt` — added `if(TARGET im::user_service) add_subdirectory(user)`

## Service Core API Surface

### Namespace

All types in `im::service::user`.

### PasswordHasher

```
class PasswordHasher {
    explicit PasswordHasher(int iterations = 10000);
    std::string hash(const std::string& password);
    bool verify(const std::string& password, const std::string& stored_hash);
};
```

- Algorithm: PBKDF2-HMAC-SHA256
- Format: `pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>`
- Salt: 16 random bytes (via `RAND_bytes`)
- Hash: 32 bytes (SHA-256 output)
- Verification uses constant-time comparison (`CRYPTO_memcmp`)
- Default iteration count: 10000 (development-safe, fast for tests)

### UserRepository

```
class UserRepository {
    explicit UserRepository(std::shared_ptr<odb::pgsql::database> db);
    bool create(const User& user);
    std::optional<User> find_by_uid(const std::string& uid);
    std::optional<User> find_by_account(const std::string& account);
    bool account_exists(const std::string& account);
    bool update_last_login(const std::string& uid, int64_t last_login);
};
```

- Uses `odb::pgsql::database` directly (not the `PgSqlConnection` wrapper,
  which has pre-existing template issues).
- `create` returns `false` on duplicate (ODB/PostgreSQL unique constraint
  violation caught as `odb::exception`).
- All methods handle `odb::exception` gracefully, returning `false`/`nullopt`.

### UserService

```
class UserService {
    UserService(std::shared_ptr<odb::pgsql::database> db,
                std::unique_ptr<PasswordHasher> hasher);

    RegisterResult register_user(const RegisterRequest& request);
    LoginResult login_by_account(const std::string& account,
                                 const std::string& password,
                                 int64_t now_ms);
    std::optional<UserProfile> get_profile_by_uid(const std::string& uid);
    std::optional<UserProfile> get_profile_by_account(const std::string& account);
};
```

#### RegisterRequest / RegisterResult

```
struct RegisterRequest {
    std::string account;     // required, non-empty
    std::string password;    // required, non-empty
    std::string nickname;    // optional, defaults to account
    int64_t now_ms;          // current timestamp
};

struct RegisterResult {
    bool ok = false;
    std::string error_code;  // "EMPTY_ACCOUNT", "EMPTY_PASSWORD",
                             // "DUPLICATE_ACCOUNT", "PERSIST_FAILED", or empty on success
    std::string message;
    UserProfile profile;     // only populated on success
};
```

#### LoginResult

```
struct LoginResult {
    bool ok = false;
    std::string error_code;  // "INVALID_ACCOUNT", "WRONG_PASSWORD", or empty on success
    std::string message;
    UserProfile profile;     // only populated on success
};
```

#### UserProfile (no password_hash exposure)

```
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

#### Service behaviors

**Register**:
- Rejects empty account (`EMPTY_ACCOUNT`)
- Rejects empty password (`EMPTY_PASSWORD`)
- Rejects duplicate account (`DUPLICATE_ACCOUNT`)
- Generates UUID v4 UID (via `RAND_bytes`, no libuuid dependency)
- Defaults nickname to account if not supplied
- Sets `create_time` and `last_login` from `now_ms`
- Creates a User with password hash, not plaintext

**Login**:
- Rejects unknown account (`INVALID_ACCOUNT`)
- Rejects wrong password (`WRONG_PASSWORD`)
- Accepts correct password, updates `last_login` in DB
- Returns profile without password_hash

**Profile**:
- Fetch by UID or account
- Returns `nullopt` for missing users

## Password Hashing Format

```
pbkdf2_sha256$10000$<salt_hex_32chars>$<hash_hex_64chars>
```

Example (illustrative):
```
pbkdf2_sha256$10000$a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6$5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8
```

- Algorithm: `pbkdf2_sha256`
- Iterations: configurable, default 10000 (dev value)
- Salt: 16 random bytes, hex-encoded (32 hex chars)
- Hash: 32 bytes, hex-encoded (64 hex chars)
- Separator: `$`

## PostgreSQL/ODB Setup Assumptions

- Docker at `127.0.0.1:5432`, database `mychat`, user `mychat`, password `mychat-dev-pass`
- Test uses `CREATE TABLE IF NOT EXISTS im_users`
- Test cleanup: `DELETE WHERE account LIKE 'task4-test-%'`
- No `DROP TABLE` — safe for shared dev databases
- ODB 2.5.0 runtime must be built via `scripts/build_odb_runtime_2_5.sh`

## Build and Test Commands

### ODB-enabled build

```bash
cmake -S . -B /tmp/mychat-task4-build \
  -DVCPKG_MANIFEST_FEATURES=pgsql-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DMYCHAT_BUILD_PGSQL_ODB=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task4-build --target im_user_service test_user_service -j2
ctest --test-dir /tmp/mychat-task4-build -R 'UserServiceCoreTest|ODBUserPersistenceTest|AuthTokenTest|RedisHiredisTest' --output-on-failure
```

### No-ODB baseline

```bash
cmake -S . -B /tmp/mychat-task4-no-odb \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=OFF \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-task4-no-odb --target test_auth_tokens test_redis_hiredis gateway_server -j2
ctest --test-dir /tmp/mychat-task4-no-odb -R 'AuthTokenTest|RedisHiredisTest' --output-on-failure
```

## Test Results

**ODB-enabled build** (4 tests, 0 failed):

```text
Test project /tmp/mychat-task4-build
    Start 1: RedisHiredisTest ........... Passed
    Start 2: ODBUserPersistenceTest .... Passed
    Start 3: UserServiceCoreTest ....... Passed
    Start 4: AuthTokenTest ............. Passed
100% tests passed, 0 tests failed out of 4
```

**No-ODB baseline** (2 tests, 0 failed):

```text
Test project /tmp/mychat-task4-no-odb
    Start 1: RedisHiredisTest ........... Passed
    Start 2: AuthTokenTest .............. Passed
100% tests passed, 0 tests failed out of 2
```

## UserServiceCoreTest Cases

1. **RegisterCreatesUserWithHashNotPlaintext** — verifies stored hash differs
   from plaintext password and matches expected format.
2. **DuplicateAccountRejected** — second register with same account returns
   `DUPLICATE_ACCOUNT`.
3. **LoginSucceedsAndUpdatesLastLogin** — successful login updates `last_login`
   in the persistent store.
4. **LoginFailsWithWrongPassword** — incorrect password returns
   `WRONG_PASSWORD`.
5. **ProfileLookupByUidAndAccount** — profile accessible by UID and account;
   missing lookups return `nullopt`.

## Known Limitations Before Gateway Integration

- `PasswordHasher` uses a default iteration count of 10000, which is appropriate
  for fast tests but insufficient for production. Production should use 600000+
  iterations.
- UUID generation via `RAND_bytes` is non-blocking and fast, but does not use
  `/dev/urandom`-exclusive source on all platforms. Sufficient for development.
- No rate limiting, captcha, or password policy enforcement.
- No gRPC/HTTP transport layer — service is a C++ class, not a server.
- `PgSqlConnection` wrapper still has pre-existing template issues (string-ID
  `std::to_string`, raw-pointer vs shared_ptr). User Service bypasses it by
  using `odb::pgsql::database` directly.
- `services/CMakeLists.txt` must have `TARGET im::user_odb` available (it is
  added from root `CMakeLists.txt` before services are processed).
