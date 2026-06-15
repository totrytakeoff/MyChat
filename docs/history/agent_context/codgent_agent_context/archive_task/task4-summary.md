# Task 4 Summary: User Service Core MVP Baseline

## Status

Ready for review.

## Implemented

### Password Hasher

- `services/user/password_hasher.hpp` / `.cpp`
- PBKDF2-HMAC-SHA256 via OpenSSL (`PKCS5_PBKDF2_HMAC`, `EVP_sha256`)
- Format: `pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>`
- Salt: 16 random bytes (`RAND_bytes`), Hash: 32 bytes, Iterations: 10000 (dev default)
- Constant-time verification (`CRYPTO_memcmp`)
- No new crypto dependencies

### User Repository

- `services/user/user_repository.hpp` / `.cpp`
- ODB-backed CRUD using `odb::pgsql::database` directly (bypasses `PgSqlConnection` wrapper)
- Methods: `create`, `find_by_uid`, `find_by_account`, `account_exists`, `update_last_login`
- Catches `odb::exception` gracefully for duplicates

### User Service

- `services/user/user_service.hpp` / `.cpp`
- `UserService` class with: `register_user`, `login_by_account`, `get_profile_by_uid`, `get_profile_by_account`
- DTO structs: `RegisterRequest`, `RegisterResult`, `LoginResult`, `UserProfile`
- `UserProfile` does not expose `password_hash`
- UUID v4 UID generation via `RAND_bytes` (no libuuid dependency)
- Register: rejects empty account/password, rejects duplicates, defaults nickname
- Login: validates credentials, updates `last_login`
- Profile: by UID or account; `nullopt` for missing

### Build Integration

- `services/user/CMakeLists.txt` — explicit source list, links `im::user_odb`, `OpenSSL::Crypto`
- `services/CMakeLists.txt` — replaced GLOB with `add_subdirectory(user)` gated on `TARGET im::user_odb`
- `test/user/CMakeLists.txt` — test target, links `im::user_service`, GTest
- `test/CMakeLists.txt` — added `add_subdirectory(user)` when `TARGET im::user_service`

### Tests

- `test/user/test_user_service.cpp` — 5 test cases (`UserServiceCoreTest`)
  1. RegisterCreatesUserWithHashNotPlaintext
  2. DuplicateAccountRejected
  3. LoginSucceedsAndUpdatesLastLogin
  4. LoginFailsWithWrongPassword
  5. ProfileLookupByUidAndAccount
- Data isolation: `CREATE TABLE IF NOT EXISTS`, cleanup by `account LIKE 'task4-test-%'`
- No `DROP TABLE`

### Documentation

- `docs/devlog/phase4_user_service_core.md` — full devlog
- `docs/devlog/current_progress.md` — updated baseline, completed, and next steps
- `docs/architecture/service_mvp_roadmap.md` — Phase D marked complete with implementation details

## Verification

### ODB-enabled build (4/4 passed)

```text
RedisHiredisTest ........... Passed
ODBUserPersistenceTest .... Passed
UserServiceCoreTest ....... Passed
AuthTokenTest ............. Passed
```

### No-ODB baseline (2/2 passed)

```text
RedisHiredisTest ........... Passed
AuthTokenTest .............. Passed
```

## Known Limitations

- PasswordHasher uses 10000 iterations (fast for tests; production needs 600000+)
- UUID via `RAND_bytes` is sufficient for dev but not the most entropy-hardened source
- No rate limiting, captcha, or password policy
- No gRPC/HTTP transport (service is a C++ class)
- `PgSqlConnection` template issues (string ID `std::to_string`, shared_ptr vs raw) remain

## Files Created/Modified

**Created:**
- `services/user/password_hasher.hpp`
- `services/user/password_hasher.cpp`
- `services/user/user_repository.hpp`
- `services/user/user_repository.cpp`
- `services/user/user_service.hpp`
- `services/user/user_service.cpp`
- `services/user/CMakeLists.txt`
- `test/user/CMakeLists.txt`
- `test/user/test_user_service.cpp`
- `docs/devlog/phase4_user_service_core.md`
- `docs/agent_context/task4-summary.md`

**Modified:**
- `services/CMakeLists.txt`
- `test/CMakeLists.txt`
- `docs/devlog/current_progress.md`
- `docs/architecture/service_mvp_roadmap.md`
