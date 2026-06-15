# Task 5 Summary: Service Build Gating and Active Test Hygiene

## Status

Ready for review.

## Changes Made

### CMake Options (Root CMakeLists.txt)

- `MYCHAT_BUILD_USER_SERVICE` (default ON) — Build User Service target.
- `MYCHAT_BUILD_CODEC_SERVICE` (default OFF) — Build legacy codec gRPC service.
- `MYCHAT_BUILD_LEGACY_UNIT_TESTS` (default OFF) — Build older unit tests pending cleanup.
- Removed ad-hoc `find_package(gRPC CONFIG QUIET)` from root — moved into services/.

### services/CMakeLists.txt

**Codec Service**:
- Gates on `MYCHAT_BUILD_CODEC_SERVICE=ON`.
- If ON + gRPC available → builds with `gRPC::grpc++` linkage.
- If ON + gRPC unavailable → `FATAL_ERROR` with actionable message.
- If OFF → clear `STATUS` message with enable instructions.

**User Service**:
- Gates on `MYCHAT_BUILD_USER_SERVICE AND TARGET im::user_odb`.
- Clear status when skipped with guidance on enabling ODB.

### test/CMakeLists.txt

- `network/` → gated on `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.
- `router/` → gated on `MYCHAT_BUILD_LEGACY_UNIT_TESTS AND TARGET im::gateway_core`.
- `utils/` → gated on `MYCHAT_BUILD_LEGACY_UNIT_TESTS`.
- `gateway/` → remains behind existing `MYCHAT_BUILD_LEGACY_GATEWAY_TESTS`.

### Task 4 Review Findings Also Fixed

1. **Codec stale gRPC build** — fully gated with proper `gRPC::grpc++` linkage.
2. **`update_last_login` return value** — checked; reverts in-memory value on DB
   write failure.
3. **Password hasher parser** — `FromHex` now validates `std::isxdigit`;
   `verify` rejects extra trailing fields (`iss.peek() != EOF`).

## Verification

| Scenario | Config | Build | Tests |
|----------|--------|-------|-------|
| ODB-enabled full build | `MYCHAT_BUILD_SERVICES=ON MYCHAT_BUILD_PGSQL_ODB=ON` | ✅ All targets | 4/4 ✅ |
| No-ODB baseline | `MYCHAT_BUILD_SERVICES=ON MYCHAT_BUILD_PGSQL_ODB=OFF` | ✅ All targets | 2/2 ✅ |
| Codec explicit ON | `MYCHAT_BUILD_CODEC_SERVICE=ON` + gRPC available | ✅ im_codec_service | N/A |
| Codec explicit ON (no gRPC) | Same + gRPC missing | `FATAL_ERROR` at configure | N/A |

## Active Baseline (Default ON, Default Build)

- `RedisHiredisTest`
- `AuthTokenTest`
- `ODBUserPersistenceTest` (when ODB enabled)
- `UserServiceCoreTest` (when ODB enabled)

## Files Changed

**Modified:**
- `CMakeLists.txt` — 3 new options, removed ad-hoc gRPC find
- `services/CMakeLists.txt` — explicit gating for codec/user services
- `test/CMakeLists.txt` — explicit gating for legacy tests
- `docs/devlog/current_progress.md` — updated
- `docs/architecture/service_mvp_roadmap.md` — Phase E ready to start

**Created:**
- `docs/devlog/phase5_build_gating.md`
- `docs/agent_context/task5-summary.md`

## Next Action

Proceed to Phase E: Gateway-to-User Service Integration.
