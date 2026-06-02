---
type: todo
status: active
updated_by: coder
---

# Todo

## Current

- [ ] Message Service MVP (Phase F) — create `services/message`, ODB schema, send/offline/history workflows, Gateway integration.

## Next

- [ ] Regenerate codec/gRPC artifacts (prerequisite for Message Service inter-service calls, or decide to follow same direct-integration pattern).
- [ ] Fix `pgsql_conn.hpp` template wrapper string-ID handling (if it becomes a blocker for new service development).
- [ ] Add connection pool to Redis wrapper before load/performance testing.

## Blocked

- ODB 2.5.0 runtime build from source — not yet available in vcpkg; CMake fails at configure time if runtime is missing.
- Stale codec/gRPC generated files in `services/codec` — gated behind `MYCHAT_BUILD_CODEC_SERVICE=OFF`; must be regenerated with correct protoc/gRPC versions.
